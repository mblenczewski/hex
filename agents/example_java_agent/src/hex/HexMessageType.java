package hex;

public enum HexMessageType {
	START(0),
	MOVE(1),
	SWAP(2),
	END(3);

	public final int value;

	private HexMessageType(int value) {
		this.value = value;
	}
}
