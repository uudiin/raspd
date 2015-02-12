package com.gfs.planecontroller;

import android.os.Handler;
import android.os.Message;

public class PiMessageHandler extends Handler {
	private IControllerListener mListener;
	
	
	public PiMessageHandler(IControllerListener lis) {
		this.mListener = lis;
	}
	
	@Override
	public void handleMessage(Message msg) {
		if(null != mListener) {
			mListener.onControllerEvent(1, 1, msg.obj);
		}
	}
	
	public void handlePiMessage(String message) {
		Message msg = Message.obtain();
		msg.obj = message;
		this.sendMessage(msg);
	}
	
	public interface IControllerListener {
		void onControllerEvent(int event, int type, Object data);
	}
}
