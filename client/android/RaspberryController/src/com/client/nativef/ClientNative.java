package com.client.nativef;

public class ClientNative {
	static {
		System.loadLibrary("client");
	}
	
	public static native void up();
	public static native void down();
	public static native void left();
	public static native void right();
	public static native void brake();
	public static native void speedup();
	public static native void speeddown();
	public static native void tleft();
	public static native void tright();
	public static native void telev();
	public static native void fire();
	public static native int connect(String host, int port);
}