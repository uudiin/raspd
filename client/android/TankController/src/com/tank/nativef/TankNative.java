package com.tank.nativef;

public class TankNative {
	static {
		System.loadLibrary("tank-jni");
	}
	
	public static native void up();
	public static native void down();
	public static native void left();
	public static native void right();
	public static native void speedup();
	public static native void speeddown();
	public static native int connect(String host, int port);
}
