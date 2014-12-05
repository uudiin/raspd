package com.raspberry.controller;


import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import com.client.nativef.ClientNative;

public class MainActivity extends ActionBarActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        Button mSupBtn = (Button)findViewById(R.id.button_sup);
        mSupBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.speedup();
			}
		});

        Button mSdownBtn = (Button)findViewById(R.id.button_sdown);
        mSdownBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.speeddown();
			}
		});

        Button mUpBtn = (Button)findViewById(R.id.button_up);
        mUpBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.up();
			}
		});

        Button mLeftBtn = (Button)findViewById(R.id.button_left);
        mLeftBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.left();
			}
		});

        Button mDownBtn = (Button)findViewById(R.id.button_down);
        mDownBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.down();
			}
		});

        Button mRightBtn = (Button)findViewById(R.id.button_right);
        mRightBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.right();
			}
		});

        Button mBrakeBtn = (Button)findViewById(R.id.button_brake);
        mBrakeBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.brake();
			}
		});

        Button mTLeftBtn = (Button)findViewById(R.id.button_tleft);
        mTLeftBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.tleft();
			}
		});

        Button mTRightBtn = (Button)findViewById(R.id.button_tright);
        mTRightBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.tright();
			}
		});

        Button mTElevBtn = (Button)findViewById(R.id.button_telev);
        mTElevBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.telev();
			}
		});

        Button mFireBtn = (Button)findViewById(R.id.button_fire);
        mFireBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				ClientNative.fire();
			}
		});

        final EditText mHostEditor = (EditText)findViewById(R.id.edit_addr);
        mHostEditor.setText("10.0.2.2");
        Button mConnectBtn = (Button)findViewById(R.id.button_connect);
        mConnectBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				String host = mHostEditor.getEditableText().toString();
				int err = ClientNative.connect(host, 8888);
				if(0 == err) {
					Toast.makeText(getApplicationContext(), "Connect success", Toast.LENGTH_SHORT).show();
				}
				else {
					Toast.makeText(getApplicationContext(), "Error " + err, Toast.LENGTH_SHORT).show();
				}
			}
		});

    }
    
    public void DisplayOutput(String msg) {
    	Toast.makeText(getApplicationContext(), "YY", Toast.LENGTH_SHORT).show();
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        if (id == R.id.action_settings) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
