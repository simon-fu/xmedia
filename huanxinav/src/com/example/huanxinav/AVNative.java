package com.example.huanxinav;

import java.io.FileDescriptor;

import android.app.Activity;
import android.content.Context;
import android.util.Log;

import com.example.huanxinav.IGxStatusCallback;

public class AVNative {

	public AVNative() {
		// LoadLibraries();
	}

	native int nativeVoeClient_Register(IGxStatusCallback  callback,Context  context,int local_port, String local_addr,int server_port,
			String mediaserver_addr, String conferenceId, short channelId,
			String password,boolean is_conference,int opus_rate_option);

	native int nativeVoeClient_FullDuplexSpeech(String conferenceId);

	native int nativeVoeClient_Stop(String conferenceId);

	native int nativeVoeClient_Release(String conferenceId);
	
	native int nativeVoeClient_GetAudioLevel(String conferenceId);
	
	native int nativeVoeClient_StartRecodeMic(IGxStatusCallback  callback,Context  context,String conferenceId,String filename);
	
	native int nativeVoeClient_StopRecodeMic(String conferenceId);


	// String tag = "SMACK-Voe";
	final static String TAG = AVNative.class.getSimpleName();

	public int register(IGxStatusCallback callback,Context  context,int local_port, String local_addr,int server_port,
			String mediaserver_addr, String conferenceId, short channelId,
			String password,boolean is_conference,int opus_rate_option) {
		int index;
		Log.v(TAG, "VoeEngine register local_port = " + local_port);
		Log.v(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! " + conferenceId
				+ ";" + conferenceId.length());
		// String password = "abc123abc";
		Log.v("PASSWORD", "register with password:" + password);
		index = nativeVoeClient_Register(callback,context,local_port,local_addr, server_port,
				mediaserver_addr, conferenceId.trim(), channelId, password,is_conference,opus_rate_option);
		Log.v(TAG, "VoeEngine register have registered index:" + index
				+ "conferenceId:" + conferenceId);
		return index;
	};

	public int unregister(String conferenceId) {
		Log.v(TAG, "VoeEngine unregister conferenceId:" + conferenceId);
		return nativeVoeClient_Release(conferenceId);
	};

	public int stop(String conferenceId) {
		Log.v(TAG, "VoeEngine stop conferenceId:" + conferenceId);
		return nativeVoeClient_Stop(conferenceId);
	}

	public int setFullDuplexSpeech(String conferenceId) {
		Log.v(TAG, "VoeEngine setFullDuplexSpeech conferenceId:" + conferenceId);
		return nativeVoeClient_FullDuplexSpeech(conferenceId);
	}
	
	public int GetAudioLevel(String conferenceId) {
		Log.v(TAG, "VoeEngine setFullDuplexSpeech conferenceId:" + conferenceId);
		return nativeVoeClient_GetAudioLevel(conferenceId);
	}

	native int nativeStartVideo(int local_port,int server_port,String mediaserver_addr, String conferenceId, short channelId,
			String password,int width,int height,int bitrate);
	native void nativeStopVideo(); 
	native int nativeProcessYUV(int width,int height,byte[] data);
 
	
	
	
	
	
	public native void nativeInit(Activity play_callback);

	public native void nativeQuit();

	public native void nativePause();

	public native void nativeResume();

	public native void onNativeResize(int x, int y, int format);

	public native void onNativeKeyDown(int keycode);

	public native void onNativeKeyUp(int keycode);

	public native void onNativeTouch(int touchDevId,
			int pointerFingerId, int action, float x, float y, float p);

	public native void onNativeAccel(float x, float y, float z);

	
	

	static {
		
		System.loadLibrary("base");
		System.loadLibrary("render");
		System.loadLibrary("easemob_jni");
	}


}
