package a3t.explorer;

class RWRecord {
	int id;
	int fldId;

	RWRecord(int id, int fldId)
	{
		this.id = id;
		this.fldId = fldId;
	}

	public boolean equals(Object other) {
		if(!(other instanceof RWRecord))
			return false;
		RWRecord otherRecord = (RWRecord) other;
		return this.id == otherRecord.id && this.fldId == otherRecord.fldId;
	}

	public int hashCode() {
		return (id * fldId) % 13;
	}
}
