package hex;

public enum HexPlayer {
	BLACK(0),
	WHITE(1);

	public final int value;

	private HexPlayer(int value) {
		this.value = value;
	}

	public static HexPlayer fromRaw(int raw) {
		switch (raw) {
		case 0 -> { return HexPlayer.BLACK; }
		case 1 -> { return HexPlayer.WHITE; }
		default -> { return null; }
		}
	}
}
