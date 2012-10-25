package edu.gatech.appInst;

public final class Config {
	public final String inJars;
	public final String outJar;
	public final String libJars;

	private static Config config;
	
	public static Config g() {
		if (config == null)
			config = new Config();
		return config;
	}
	
	private Config() {
		inJars = System.getProperty("a3t.in.jars", "libs/app.jar");
		outJar = System.getProperty("a3t.out.jar", "output/intrumentedApp.jar");
        libJars = System.getProperty("a3t.lib.jars", "libs/core.jar:libs/ext.jar:libs/junit.jar:libs/bouncycastle.jar:bin:libs/android.jar");
	}
}
