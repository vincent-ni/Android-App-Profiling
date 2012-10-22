package a3t.instrumentor;

public final class Config {
	public final String inJars;
	public final String outJar;
	public final String libJars;
	public final String inputMethsFile;
    public final String modelMethsFile;
	public final String outDir;
	public final String sdkDir;
    public final RWKind rwKind;
	public final String fldsWhitelist;
	public final String fldsBlacklist;
	public final String methsWhitelist;
	public final boolean instrAllFields;

	private static Config config;

	public static Config g() {
		if (config == null)
			config = new Config();
		return config;
	}

	private Config() {
		inJars = System.getProperty("a3t.in.jars", null);
		outJar = System.getProperty("a3t.out.jar", null);
        libJars = System.getProperty("a3t.lib.jars", null);
		inputMethsFile = System.getProperty("a3t.inputmeths.file", null);
		modelMethsFile = System.getProperty("a3t.modelmeths.file", null);
		outDir = System.getProperty("a3t.out.dir", null);
		sdkDir = System.getProperty("a3t.sdk.dir", null);
		String s = System.getProperty("a3t.rw.kind", "id_field_write");
		if (s.equals("none")) 
			rwKind = RWKind.NONE;
		else if (s.equals("id_field_write"))
			rwKind = RWKind.ID_FIELD_WRITE;
		else if (s.equals("explicit_write"))
			rwKind = RWKind.EXPLICIT_WRITE;
		else if (s.equals("only_write"))
			rwKind = RWKind.ONLY_WRITE;
		else
			throw new RuntimeException("Unknown value for a3t.rw.kind: " + s);
		
		fldsWhitelist = System.getProperty("a3t.whiteflds.file", null);
		fldsBlacklist = System.getProperty("a3t.blackflds.file", null);
		methsWhitelist = System.getProperty("a3t.whitemeths.file", null);
		instrAllFields = Boolean.getBoolean("a3t.instrflds.all");

		System.out.println("a3t.in.jars=" + inJars);
		System.out.println("a3t.out.jar=" + outJar);
		System.out.println("a3t.lib.jars=" + libJars);
		System.out.println("a3t.inputmeths.file=" + inputMethsFile);
		System.out.println("a3t.modelmeths.file=" + modelMethsFile);
		System.out.println("a3t.out.dir=" + outDir);
		System.out.println("a3t.sdk.dir=" + sdkDir + " (SDK if null)");
		System.out.println("a3t.rw.kind=" + rwKind);
		System.out.println("a3t.whiteflds.file=" + fldsWhitelist);
		System.out.println("a3t.blackflds.file=" + fldsBlacklist);
		System.out.println("a3t.whitemeths.file=" + methsWhitelist);
		System.out.println("a3t.instrflds.all="+ instrAllFields);
	}

	public boolean isSDK() {
		// somewhat counterintuitive: we are instrumenting sdk if
		// sdkDir is null.
		return sdkDir == null;
	}
}

