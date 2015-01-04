package com.client.nativef;

import com.raspberry.controller.PiMessageHandler;

public class ClientNative {
	static {
		System.loadLibrary("client");
	}
	
	public static native void selecttank();
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
	public static native void setJNIEnv(PiMessageHandler mMessageHandler);
}