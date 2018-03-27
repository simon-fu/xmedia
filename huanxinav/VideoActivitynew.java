package com.example.huanxinav;

import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.os.Bundle;
import android.app.Activity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup.LayoutParams;
import javax.microedition.khronos.egl.*;
import android.content.*;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.view.*;
import android.os.*;
import android.util.Log;
import android.graphics.*;
import android.hardware.Camera.Parameters;
import java.io.IOException;
import com.example.huanxinav.AVNative;

public class VideoActivity extends Activity implements SurfaceHolder.Callback,IGxStatusCallback,Camera.PreviewCallback {
	static AVNative nativeAgt;
	int voe_index = -1;
	static String remote_addr = "10.0.1.46";//绾㈢背ip
	static int vplayport = 9000;
	static int vcaptureport = 10000;
	int aport = 7000;
	static String cid = "20130814172059902001ce53c2646851";
	static String password = "";
	static short channelm = 0;
	short channelm_a = 3;
    private static VideoActivity mSingleton;
    private static SDLSurface mSurface;
    static int screenWidth = 0; 
    static int screenHeight = 0;    
    private SurfaceView mSurfaceview = null; // 
    private SurfaceHolder mSurfaceHolder = null;
    private Camera mCamera = null;
	private byte yuv_frame[];
	private byte yuv_Rotate90[];
	private Parameters mParameters;
	private int yuv_cnt = 0;
	private int mwidth = 640;
	private int mheight = 480;
	private int bitrate = 1024;	
	private boolean start_flag = false;
	int camera_count;
    // This is what SDL runs in. It invokes SDL_main(), eventually
    private static Thread mSDLThread;
    // EGL private objects
    public static EGLContext  mEGLContext;
    public static EGLSurface  mEGLSurface;
    public static EGLDisplay  mEGLDisplay;
    public static EGLConfig   mEGLConfig;
    private static int mGLMajor;
   
    private static VideoActivity instance = null;
	static VideoActivity getInstance() {
		return instance;
	}
    // Setup
    protected void onCreate(Bundle savedInstanceState) {
        Log.v("SDL", "onCreate()");
        //requestWindowFeature(Window.FEATURE_NO_TITLE); 
        super.onCreate(savedInstanceState);    
        Window win = getWindow();
		win.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		win.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);
		setContentView(R.layout.activity_video_capture);
        mSingleton = this;
        nativeAgt = new AVNative();
        mSurfaceview = (SurfaceView) findViewById(R.id.surface_x264codecview); 
        mSurfaceHolder = mSurfaceview.getHolder(); 
        mSurfaceHolder.addCallback(this);
        
        mSurface = new SDLSurface(getApplication());
      
        screenWidth = getWindowManager().getDefaultDisplay().getWidth();  
        screenHeight = getWindowManager().getDefaultDisplay().getHeight(); // 灞忓箷楂橈紙鍍忕礌锛屽锛�800p锛�  
        //setContentView(mSurface);
        LayoutParams layoutparam = new LayoutParams(screenWidth, screenHeight);
        Log.e("SDL","screenWidth:"+screenWidth +"screenHeight"+screenHeight);
        addContentView(mSurface, layoutparam);
        //setContentView(mSurface);
       
        SurfaceHolder holder = mSurface.getHolder();        	
        holder.setFixedSize(screenWidth,screenHeight);
        //setRequestedOrientation(
			//	ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        if((voe_index = nativeAgt.register(this,this.getApplicationContext(),aport, "0.0.0.0",aport, remote_addr, cid, channelm_a,password,false,1)) != -1)
		{
			Log.v("VOE", "VOE Register OK");
		}
		else{
			Log.v("VOE", "VOE Register Failed");
		}
		if(nativeAgt.setFullDuplexSpeech(cid)==1)
		{
			Log.v("VOE", "VOE setFullDuplexSpeech OK");
		}
		else{
			Log.v("VOE", "VOE setFullDuplexSpeech Failed");
		}
		instance = this;
    }
    
    public  boolean isScreenOriatationPortrait() {
    	return false;
    	 //return this.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
    }
    
    @Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		Log.e("SDL","pzy onConfigurationChanged");
		if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
			Log.d("SDL", "onConfigurationChanged landscape");
		} else {
			Log.e("SDL", "onConfigurationChanged portrait");
		}
		if(start_flag == true)
	    {
			start_flag = false;
	        Log.e("SDL","to release x264 encoder");
	    	nativeAgt.Nativex264_releaseencoder();
	    	Log.e("SDL","have release x264 encoder");
	    
	    }
		startcapture();

	}
    
    void YUV420spRotate90(byte[]  dst, byte[] src, int srcWidth, int srcHeight)  
    {  
        int nWidth = 0, nHeight = 0;  
        int wh = 0;  
        int uvHeight = 0;  
        if(srcWidth != nWidth || srcHeight != nHeight)  
        {  
            nWidth = srcWidth;  
            nHeight = srcHeight;  
            wh = srcWidth * srcHeight;  
            uvHeight = srcHeight >> 1;//uvHeight = height / 2  
        }  
      
        //鏃嬭浆Y  
        int k = 0;  
        for(int i = 0; i < srcWidth; i++){  
            int nPos = srcWidth - 1;  
            for(int j = 0; j < srcHeight; j++)  
            {  
                dst[k] = src[nPos - i];  
                k++;  
                nPos += srcWidth;  
            }  
        }  
      
        for(int i = 0; i < srcWidth; i+=2){  
            int nPos = wh + srcWidth - 1;  
            for(int j = 0; j < uvHeight; j++) {  
                dst[k] = src[nPos - i - 1];  
                dst[k + 1] = src[nPos - i];  
                k += 2;  
                nPos += srcWidth;  
            }  
        }  
        return;  
    }  
    
    void startcapture()
    {
    	 if(start_flag == false )
 		{	
         	if(isScreenOriatationPortrait()){
         		Log.e("SDL","pzy 绔栧睆閲囬泦");
         		nativeAgt.Nativex264_getencoder(this,vcaptureport, vplayport, remote_addr, cid, channelm_a,mheight,mwidth,bitrate,password);
         	}
         	else{
         		Log.e("SDL","pzy  妯睆閲囬泦");
         		nativeAgt.Nativex264_getencoder(this,vcaptureport, vplayport, remote_addr, cid, channelm_a,mwidth,mheight,bitrate,password);		
         	}
         	start_flag = true;
 		}    
    	 try { 
 	        if (mCamera == null) {
 				//mCamera = Camera.open();
 	        	camera_count = Camera.getNumberOfCameras();
 	        	Log.e("huanxin","camera count:"+camera_count);
 	        	if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
 	        		   for (int i = 0; i <camera_count;  i++) {
 	        		    CameraInfo info = new CameraInfo();
 	        		    Camera.getCameraInfo(i, info);
 	        		    if (info.facing == CameraInfo.CAMERA_FACING_FRONT) {//杩欏氨鏄墠缃憚鍍忓ご銆�
 	        		    	Log.e("huanxin","to open front camera");
 	        		    	mCamera = Camera.open(i);
 	        		    }
 	        		    }
 	        		   }
 	        	if (mCamera == null) {
 	        		Log.e("HUANXIN","AAAAA OPEN camera");
	        			  mCamera = Camera.open();
	        		  }
 	
 			}
 	       
 			mCamera.stopPreview();
 			mParameters = mCamera.getParameters();
 			if(isScreenOriatationPortrait())
 			{
 				mCamera.setDisplayOrientation(90);
 			}
 			
 			mParameters.setPreviewSize(mwidth, mheight); 			
 			mParameters.setPreviewFrameRate(25); // 绱㈠凹鐧借壊鎵嬫満涓嶆敮鎸佸抚鐜�25
 			mCamera.setParameters(mParameters);
 			int mformat = mParameters.getPreviewFormat();
 			int bitsperpixel = ImageFormat.getBitsPerPixel(mformat);
 			Log.e("huanxin", "pzy bitsperpixel: " + bitsperpixel);
 			yuv_frame = new byte[mwidth * mheight * bitsperpixel / 8];
 			yuv_Rotate90 = new byte[mwidth * mheight * bitsperpixel / 8];
 			mCamera.addCallbackBuffer(yuv_frame);
 			//mCamera.setPreviewDisplay(holder);
 			mCamera.setPreviewDisplay(mSurfaceHolder);
 			mCamera.setPreviewCallbackWithBuffer(this);
 			
 			mCamera.startPreview();
 			Log.e("SDL", "camera start preview");
 	        }catch (IOException e) { 
 	        	e.printStackTrace(); 
 	            }  
    }
    
    @Override
	public void onPreviewFrame(byte[] data, Camera camera) {
		 yuv_cnt++;
		 //Log.e("SDL", "pzy data len" + data.length + " yuv_cnt: "+
		 //yuv_cnt);
		 if(start_flag == true){
			 if(isScreenOriatationPortrait())
			 {
				 Log.e("SDL","call YUV420spRotate90");
				 YUV420spRotate90(yuv_Rotate90,yuv_frame,  mwidth,mheight);
				 nativeAgt.Nativex264_encode264(yuv_Rotate90, mheight,mwidth);				 
			 }
			 else
			 {		
				 nativeAgt.Nativex264_encode264(yuv_frame, mwidth,mheight);
			 }
		}
		// mCamera.addCallbackBuffer(yuv_frame);
		camera.addCallbackBuffer(yuv_frame);
	}
    

    // Events
    protected void onPause() {
        Log.e("SDL", "onPause()");
        super.onPause();
    }

    public void onResume() {
        Log.e("SDL", "pzy onResume()");
        super.onResume();
       
      
    }
    protected void onDestroy() {
        
        Log.e("SDL", "onDestroy()");
        nativeAgt.stop(VideoActivity.cid);
        Log.e("SDL","to stop play");
        nativeAgt.StopVideoPlay();
        Log.e("SDL","Have stop play");
       
        Log.e("SDL","to stop mSDLThread");
        if (mSDLThread != null) {
            try {
                mSDLThread.join();
            } catch(Exception e) {
                Log.e("SDL", "Problem stopping thread: " + e);
            }
            mSDLThread = null;
            Log.e("SDL", "Finished waiting for SDL thread");
        } 
        Log.e("SDL","have stopped mSDLThread");
    	
        if(start_flag == true)
    	{
        	nativeAgt.unregister(cid);
        	Log.e("SDL","to release x264 encoder");
    		nativeAgt.Nativex264_releaseencoder();
    		Log.e("SDL","have release x264 encoder");
    	}
    	 if (mCamera != null) {
    		 mCamera.setPreviewCallback(null); 
     		 mCamera.stopPreview(); 
             mCamera.release(); 
             mCamera = null; 
         }
    	 super.onDestroy();       
    }
    
    @Override 
    public void surfaceCreated(SurfaceHolder holder) {
    	Log.e("Video_Capature", "call surfaceCreated " );    	 
    } 

    @Override 
    public void surfaceDestroyed(SurfaceHolder holder) { 
    	Log.e("Video_Capature", "call surfaceDestroyed " );
    	Log.e("SDL", "surfaceDestroyed()");
       
    } 
    
    @Override 
    public void surfaceChanged(SurfaceHolder holder, int format, int width_size,int height_size) { 
    	 Log.e("Video_Capature", "pzy call surfaceChanged " +"width:"+ mwidth +  "height:"+mheight );	 
    	 startcapture();
    } 
    
    
    public void updateStatus(int status) {

		Log.v("huanxin", "pzy call updateStatus: " + status);
		return;
	}
    
    static int COMMAND_CHANGE_TITLE = 1;
    // Handler for the messages
    Handler commandHandler = new Handler() {
        public void handleMessage(Message msg) {
            if (msg.arg1 == COMMAND_CHANGE_TITLE) {
                setTitle((String)msg.obj);
            }
        }
    };

    // Send a message from the SDLMain thread
    void sendCommand(int command, Object data) {
        Message msg = commandHandler.obtainMessage();
        msg.arg1 = command;
        msg.obj = data;
        commandHandler.sendMessage(msg);
    }   
   
    public static void releaseEGL()
    {
    	EGL10 egl = (EGL10)EGLContext.getEGL();        
        if (mEGLSurface != null) 
        {  
        	egl.eglMakeCurrent(mEGLDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);  
        	egl.eglDestroySurface(mEGLDisplay, mEGLSurface);  
        	mEGLSurface = null;  
        }  
        if (mEGLContext != null) 
        {
        	egl.eglDestroyContext(mEGLDisplay, mEGLContext);  
        	mEGLContext = null;  
        }  
        if (mEGLDisplay != null) {  
        	egl.eglTerminate(mEGLDisplay);  
        	mEGLDisplay = null;  
        }  
    }    
    // Java functions called from C

    public static boolean createGLContext(int majorVersion, int minorVersion) {
    	Log.d("SDL","to call initEGL");
    	releaseEGL();
        return initEGL(majorVersion, minorVersion);
    }

    public static void flipBuffers() {
        flipEGL();
    }

    public static void setActivityTitle(String title) {
        // Called from SDLMain() thread and can't directly affect the view
        mSingleton.sendCommand(COMMAND_CHANGE_TITLE, title);
    }

    public static Context getContext() {
        return mSingleton;
    }

    public static void startApp() {
        // Start up the C app thread
        if (mSDLThread == null) {
            mSDLThread = new Thread(new SDLMain(), "SDLThread");
            mSDLThread.start();
        }
        else {
        	VideoActivity.nativeAgt.nativeResume();
        }
    }

    // EGL functions
    public static boolean initEGL(int majorVersion, int minorVersion) {
        if (mEGLDisplay == null) {
            Log.v("SDL", "initEGL" + majorVersion + "." + minorVersion);

            try {
                EGL10 egl = (EGL10)EGLContext.getEGL();
                EGLDisplay dpy = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
                int[] version = new int[2];
                egl.eglInitialize(dpy, version);
                int EGL_OPENGL_ES_BIT = 1;
                int EGL_OPENGL_ES2_BIT = 4;
                int renderableType = 0;
                if (majorVersion == 2) {
                    renderableType = EGL_OPENGL_ES2_BIT;
                } else if (majorVersion == 1) {
                    renderableType = EGL_OPENGL_ES_BIT;
                }
                int[] configSpec = {
                    //EGL10.EGL_DEPTH_SIZE,   16,
                    EGL10.EGL_RENDERABLE_TYPE, renderableType,
                    EGL10.EGL_NONE
                };
                EGLConfig[] configs = new EGLConfig[1];
                int[] num_config = new int[1];
                if (!egl.eglChooseConfig(dpy, configSpec, configs, 1, num_config) || num_config[0] == 0) {
                    Log.e("SDL", "No EGL config available");
                    return false;
                }
                EGLConfig config = configs[0];
                mEGLDisplay = dpy;
                mEGLConfig = config;
                mGLMajor = majorVersion;
                createEGLSurface();
            } catch(Exception e) {
                Log.v("SDL", e + "");
                for (StackTraceElement s : e.getStackTrace()) {
                    Log.v("SDL", s.toString());
                }
            }
        }
        else createEGLSurface();

        return true;
    }

    public static boolean createEGLContext() {
        EGL10 egl = (EGL10)EGLContext.getEGL();
        int EGL_CONTEXT_CLIENT_VERSION=0x3098;
        int contextAttrs[] = new int[] { EGL_CONTEXT_CLIENT_VERSION, mGLMajor, EGL10.EGL_NONE };
        mEGLContext = egl.eglCreateContext(mEGLDisplay, mEGLConfig, EGL10.EGL_NO_CONTEXT, contextAttrs);
        if (mEGLContext == EGL10.EGL_NO_CONTEXT) {
            Log.e("SDL", "Couldn't create context");
            return false;
        }
        return true;
    }

    public static boolean createEGLSurface() {
        if (mEGLDisplay != null && mEGLConfig != null) {
            EGL10 egl = (EGL10)EGLContext.getEGL();
            if (mEGLContext == null) createEGLContext();
            Log.v("SDL", "Creating new EGL Surface");
            EGLSurface surface = egl.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, mSurface, null);
            if (surface == EGL10.EGL_NO_SURFACE) {
                Log.e("SDL", "Couldn't create surface");
                return false;
            }
            if (!egl.eglMakeCurrent(mEGLDisplay, surface, surface, mEGLContext)) {
                Log.e("SDL", "Old EGL Context doesnt work, trying with a new one");
                createEGLContext();
                if (!egl.eglMakeCurrent(mEGLDisplay, surface, surface, mEGLContext)) {
                    Log.e("SDL", "Failed making EGL Context current");
                    return false;
                }
            }
            mEGLSurface = surface;
            return true;
        }
        return false;
    }

    // EGL buffer flip
    public static void flipEGL() {
        try {
            EGL10 egl = (EGL10)EGLContext.getEGL();
            egl.eglWaitNative(EGL10.EGL_CORE_NATIVE_ENGINE, null);
            // drawing here
            egl.eglWaitGL();
            egl.eglSwapBuffers(mEGLDisplay, mEGLSurface);
        } catch(Exception e) {
            Log.v("SDL", "flipEGL(): " + e);
            for (StackTraceElement s : e.getStackTrace()) {
                Log.v("SDL", s.toString());
            }
        }
    }   
}

/**
    Simple nativeInit() runnable
*/
class SDLMain implements Runnable {
    public void run() {
        // Runs SDL_main()
    	Log.d("SDL","TO CALL NATIVEINIT");
    	VideoActivity.nativeAgt.nativeInit(VideoActivity.getInstance());
    	Log.d("SDL","HAVE CALL NATIVEINIT");
    	VideoActivity.nativeAgt.StartVideoPlay(VideoActivity.vplayport, VideoActivity.vcaptureport, VideoActivity.remote_addr, VideoActivity.cid, VideoActivity.channelm,VideoActivity.password);
    	VideoActivity.nativeAgt.nativeQuit();
    	VideoActivity.releaseEGL();
        Log.v("SDL", "SDL thread terminated");
    }
}
/**
    SDLSurface. This is what we draw on, so we need to know when it's created
    in order to do anything useful. 
    Because of this, that's where we set up the SDL thread
*/
class SDLSurface extends SurfaceView implements SurfaceHolder.Callback
	{
	String TAG = SDLSurface.class.getSimpleName();	
    // Startup    
    public SDLSurface(Context context) {
        super(context);
        getHolder().addCallback(this); 
        
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();             
    }
    // Called when we have a valid drawing surface
    public void surfaceCreated(SurfaceHolder holder) {
        Log.e("SDL", "surfaceCreated()");       
        holder.setType(SurfaceHolder.SURFACE_TYPE_GPU);       
    }
    // Called when we lose the surface
    public void surfaceDestroyed(SurfaceHolder holder) {
        
    }

    // Called when the surface is resized
    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.e("SDL", "pzy surfaceChanged()");
        int sdlFormat = 0x85151002; // SDL_PIXELFORMAT_RGB565 by default
        switch (format) {
        case PixelFormat.A_8:
            Log.v("SDL", "pixel format A_8");
            break;
        case PixelFormat.LA_88:
            Log.v("SDL", "pixel format LA_88");
            break;
        case PixelFormat.L_8:
            Log.v("SDL", "pixel format L_8");
            break;
        case PixelFormat.RGBA_4444:
            Log.v("SDL", "pixel format RGBA_4444");
            sdlFormat = 0x85421002; // SDL_PIXELFORMAT_RGBA4444
            break;
        case PixelFormat.RGBA_5551:
            Log.v("SDL", "pixel format RGBA_5551");
            sdlFormat = 0x85441002; // SDL_PIXELFORMAT_RGBA5551
            break;
        case PixelFormat.RGBA_8888:
            Log.v("SDL", "pixel format RGBA_8888");
            sdlFormat = 0x86462004; // SDL_PIXELFORMAT_RGBA8888
            break;
        case PixelFormat.RGBX_8888:
            Log.v("SDL", "pixel format RGBX_8888");
            sdlFormat = 0x86262004; // SDL_PIXELFORMAT_RGBX8888
            break;
        case PixelFormat.RGB_332:
            Log.v("SDL", "pixel format RGB_332");
            sdlFormat = 0x84110801; // SDL_PIXELFORMAT_RGB332
            break;
        case PixelFormat.RGB_565:
            Log.v("SDL", "pixel format RGB_565");
            sdlFormat = 0x85151002; // SDL_PIXELFORMAT_RGB565
            break;
        case PixelFormat.RGB_888:
            Log.v("SDL", "pixel format RGB_888");
            // Not sure this is right, maybe SDL_PIXELFORMAT_RGB24 instead?
            sdlFormat = 0x86161804; // SDL_PIXELFORMAT_RGB888
            break;
        default:
            Log.v("SDL", "pixel format unknown " + format);
            break;
        }
        VideoActivity.nativeAgt.onNativeResize(width, height, sdlFormat); 
        Log.e("SDL", "Window size:" + width + "x"+height);
        Log.e("SDL", "Window size:" + VideoActivity.screenWidth + "X" +VideoActivity.screenHeight);
        VideoActivity.startApp();            
    }
}