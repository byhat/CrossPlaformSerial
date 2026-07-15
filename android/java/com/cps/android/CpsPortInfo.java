package com.cps.android;

/** Plain data holder returned by {@link CpsUsbSerial#enumerate()}. */
public class CpsPortInfo {
    public String name;
    public String description;
    public String manufacturer;
    public String serialNumber;
    public String systemLocation;
    public int vid;
    public int pid;
}
