package com.cps.android;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * JNI bridge between the native cps AndroidBackend and the usb-serial-for-android
 * driver library. All native-visible methods are static; the native side drives.
 *
 * Data arrival: a {@link SerialInputOutputManager} runs on its own thread and calls
 * {@link #cpsNativeOnRead(long, byte[])} which dispatches back into C++.
 */
public class CpsUsbSerial {
    private static final String TAG = "cps";
    private static final String ACTION_USB_PERMISSION = "com.cps.android.USB_PERMISSION";

    /** Set from native code right after the DEX is loaded. */
    private static volatile Context sContext;

    private static final class Entry {
        UsbSerialPort port;
        UsbDeviceConnection connection;
        SerialInputOutputManager io;
        Thread ioThread;
    }
    private static final ConcurrentHashMap<Long, Entry> sOpen = new ConcurrentHashMap<>();

    // ---- native callbacks (implemented in AndroidBackend.cpp) ----------------
    public static native void cpsNativeOnRead(long handle, byte[] data);
    public static native void cpsNativeOnError(long handle, int code);

    private CpsUsbSerial() {}

    public static void setContext(Context ctx) {
        sContext = ctx.getApplicationContext();
    }

    // ---- enumeration ---------------------------------------------------------

    public static CpsPortInfo[] enumerate() {
        Context ctx = sContext;
        if (ctx == null) return new CpsPortInfo[0];
        UsbManager mgr = (UsbManager) ctx.getSystemService(Context.USB_SERVICE);
        if (mgr == null) return new CpsPortInfo[0];

        List<UsbSerialDriver> drivers = UsbSerialProber.getDefaultProber().findAllDrivers(mgr);
        List<CpsPortInfo> out = new ArrayList<>();
        for (UsbSerialDriver d : drivers) {
            UsbDevice dev = d.getDevice();
            CpsPortInfo info = new CpsPortInfo();
            info.name = dev.getDeviceName();
            info.systemLocation = info.name;
            info.description = d.toString();
            info.manufacturer = safeManufacturer(dev);
            info.serialNumber = safeSerial(mgr, dev);
            info.vid = dev.getVendorId();
            info.pid = dev.getProductId();
            out.add(info);
        }
        return out.toArray(new CpsPortInfo[0]);
    }

    // ---- open / write / close ------------------------------------------------

    public static boolean open(long handle, String deviceName, int baud,
                               int dataBits, int parity, int stopBits, int flow) {
        try {
            Context ctx = sContext;
            if (ctx == null) return false;
            UsbManager mgr = (UsbManager) ctx.getSystemService(Context.USB_SERVICE);
            UsbSerialDriver driver = findDriver(mgr, deviceName);
            if (driver == null) return false;

            UsbDevice dev = driver.getDevice();
            if (!mgr.hasPermission(dev)) {
                if (!requestPermission(mgr, ctx, dev)) return false;
            }
            UsbDeviceConnection conn = mgr.openDevice(dev);
            if (conn == null) return false;

            UsbSerialPort port = driver.getPorts().get(0);
            port.open(conn);
            port.setParameters(baud, dataBits, mapStopBits(stopBits), mapParity(parity));

            Entry e = new Entry();
            e.port = port;
            e.connection = conn;
            e.io = new SerialInputOutputManager(port, new ReadListener(handle));
            e.ioThread = new Thread(e.io, "cps-io");
            e.ioThread.start();
            sOpen.put(handle, e);
            return true;
        } catch (Exception ex) {
            Log.e(TAG, "open failed", ex);
            return false;
        }
    }

    public static int write(long handle, byte[] data) {
        Entry e = sOpen.get(handle);
        if (e == null) return -1;
        try {
            e.port.write(data, 1000); // 1s timeout
            return data.length;
        } catch (Exception ex) {
            Log.e(TAG, "write failed", ex);
            return -1;
        }
    }

    public static void close(long handle) {
        Entry e = sOpen.remove(handle);
        if (e == null) return;
        try { if (e.io != null) e.io.stop(); } catch (Exception ignored) {}
        try { if (e.port != null) e.port.close(); } catch (Exception ignored) {}
    }

    // ---- internals -----------------------------------------------------------

    private static final class ReadListener implements SerialInputOutputManager.Listener {
        private final long handle;
        ReadListener(long h) { handle = h; }
        @Override public void onNewData(byte[] data) { cpsNativeOnRead(handle, data); }
        @Override public void onRunError(Exception e) {
            Log.e(TAG, "io error", e);
            cpsNativeOnError(handle, 1);
        }
    }

    private static UsbSerialDriver findDriver(UsbManager mgr, String deviceName) {
        if (mgr == null) return null;
        for (UsbSerialDriver d : UsbSerialProber.getDefaultProber().findAllDrivers(mgr)) {
            if (d.getDevice().getDeviceName().equals(deviceName)) return d;
        }
        return null;
    }

    /** Blocks until the user grants/denies USB permission (auto-request on open). */
    private static boolean requestPermission(final UsbManager mgr, final Context ctx, final UsbDevice dev) {
        final CountDownLatch latch = new CountDownLatch(1);
        final boolean[] granted = { false };
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent intent) {
                if (ACTION_USB_PERMISSION.equals(intent.getAction())) {
                    granted[0] = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                    latch.countDown();
                }
            }
        };
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) flags |= PendingIntent.FLAG_MUTABLE;
        Intent intent = new Intent(ACTION_USB_PERMISSION);
        intent.setPackage(ctx.getPackageName());
        PendingIntent pi = PendingIntent.getBroadcast(ctx, 0, intent, flags);
        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ctx.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            ctx.registerReceiver(receiver, filter);
        }
        mgr.requestPermission(dev, pi);
        try { latch.await(30, TimeUnit.SECONDS); } catch (InterruptedException ignored) {}
        try { ctx.unregisterReceiver(receiver); } catch (Exception ignored) {}
        return granted[0];
    }

    // usb-serial-for-android parity/stop constants differ from cps enums -> remap.
    private static int mapParity(int cpsParity) {
        // cps: 0 none,1 even,2 odd,3 space,4 mark
        switch (cpsParity) {
            case 1: return UsbSerialPort.PARITY_EVEN;
            case 2: return UsbSerialPort.PARITY_ODD;
            case 3: return UsbSerialPort.PARITY_SPACE;
            case 4: return UsbSerialPort.PARITY_MARK;
            default: return UsbSerialPort.PARITY_NONE;
        }
    }

    private static int mapStopBits(int cpsStop) {
        // cps: 0 one,1 onehalf,2 two
        switch (cpsStop) {
            case 1: return UsbSerialPort.STOPBITS_1_5;
            case 2: return UsbSerialPort.STOPBITS_2;
            default: return UsbSerialPort.STOPBITS_1;
        }
    }

    private static String safeManufacturer(UsbDevice dev) {
        try { return nullToEmpty(dev.getManufacturerName()); }
        catch (SecurityException ignored) { return ""; }
    }

    private static String safeSerial(UsbManager mgr, UsbDevice dev) {
        if (!mgr.hasPermission(dev)) return "";
        try { return nullToEmpty(dev.getSerialNumber()); }
        catch (SecurityException ignored) { return ""; }
    }

    private static String nullToEmpty(String s) { return s == null ? "" : s; }
}
