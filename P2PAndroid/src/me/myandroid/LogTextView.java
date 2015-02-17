package me.myandroid;

import android.widget.ScrollView;
import android.widget.TextView;

public class LogTextView {
	
	public LogTextView(TextView tv, ScrollView sv)
	{
		this.tv = tv;
		this.sv = sv;
		//tv.setMovementMethod(new ScrollingMovementMethod());
	}
	
	public void append(final String str)
	{
		//sendStringMessage(msgHandler, MSG_ADDTEXT, str);
		tv.post(new Runnable() {
			@Override
			public void run() {
				tv.append(str);
				scrollViewToDown(sv);
				//sv.fullScroll(ScrollView.FOCUS_DOWN); // can't work
			}
			
		});
	}
	
	public void appendln(final String str)
	{
		append(str + "\n");
	}
	
	private TextView tv = null;
	private ScrollView sv = null;
	
	
	private void scrollViewToDown(final ScrollView sv)
	{
		sv.post(new Runnable() {

			@Override
			public void run() {
				sv.fullScroll(ScrollView.FOCUS_DOWN);
			}
			
		});
	}

	

}
