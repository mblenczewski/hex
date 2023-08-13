import java.io.*;
import java.net.*;
import java.nio.*;
import java.util.*;

import hex.*;

public class Agent {
	private static Net net;
	private static State state;
	private static Board board;
	private static HexPlayer player, opponent, winner;
	private static int gameSecs, threadLimit, memLimitMib; // unused

	public static void main(String[] args) {
		if (args.length < 2) {
			System.err.println("Not enough args: <host> <port>");
			return;
		}

		String host = args[0];
		int port = Integer.parseInt(args[1]);

		try {
			net = new Net(host, port);
		} catch (IOException ex) {
			System.err.println("Failed to initialise network");
			return;
		}

		state = State.START;

		boolean gameOver = false, firstRound = true;
		while (!gameOver) {
			switch (state) {
			case START -> {
				Optional<NetMessage> msg = net.recvMsg();

				if (!msg.isPresent()) {
					System.err.println("Failed to receive message from hex server");
					return;
				}

				if (msg.get() instanceof StartMessage start) {
					player = start.player();

					gameSecs = start.gameSecs();
					threadLimit = start.threadLimit();
					memLimitMib = start.memLimitMib();

					board = new Board(start.boardSize());

					switch (player) {
					case BLACK -> { opponent = HexPlayer.WHITE; state = State.SEND; }
					case WHITE -> { opponent = HexPlayer.BLACK; state = State.RECV; }
					}
				} else {
					System.err.println("Invalid message received from server");
					return;
				}
			}

			case RECV -> {
				Optional<NetMessage> msg = net.recvMsg();

				if (!msg.isPresent()) {
					System.err.println("Failed to receive message from hex server");
					return;
				}

				if (msg.get() instanceof MoveMessage move) {
					board.play(opponent, move.boardX(), move.boardY());

					if (firstRound && new Random().nextBoolean()) {
						board.swap();

						if (!net.sendMsg(new SwapMessage())) {
							System.err.println("Failed to send message to hex server");
							return;
						}

						state = state.RECV;
					} else {
						state = state.SEND;
					}
				} else if (msg.get() instanceof SwapMessage swap) {
					board.swap();
					state = State.SEND;
				} else if (msg.get() instanceof EndMessage end) {
					winner = end.winner();
					state = State.END;
				} else {
					System.err.println("Invalid message received from server");
					return;
				}

				firstRound = false;
			}

			case SEND -> {
				Optional<Move> nextMove = board.next();

				if (!nextMove.isPresent()) {
					System.err.println("Failed to generate next board move");
					return;
				}

				Move move = nextMove.get();

				board.play(player, move.x(), move.y());

				if (!net.sendMsg(new MoveMessage(move.x(), move.y()))) {
					System.err.println("Failed to send message to hex server");
					return;
				}

				state = State.RECV;
				firstRound = false;
			}

			case END -> {
				gameOver = true;
			}
			}
		}

		return;
	}
}

enum State {
	START, RECV, SEND, END,
}

record Move(int x, int y) {}

enum Cell {
	BLACK(HexPlayer.BLACK.value),
	WHITE(HexPlayer.WHITE.value),
	EMPTY(2);

	public final int value;

	private Cell(int value) {
		this.value = value;
	}
}

class Board {
	private final int size;
	private final ArrayList<Cell> cells;
	private final ArrayList<Move> moves;

	public Board(int size) {
		this.size = size;
		this.cells = new ArrayList<>(size * size);
		this.moves = new ArrayList<>(size * size);

		for (int j = 0; j < size; j++) {
			for (int i = 0; i < size; i++) {
				this.cells.add(Cell.EMPTY);
				this.moves.add(new Move(i, j));
			}
		}

		Collections.shuffle(this.moves);
	}

	public boolean play(HexPlayer player, int x, int y) {
		int idx = y * this.size + x;

		Cell cell = this.cells.get(idx);
		if (cell != Cell.EMPTY) return false;

		switch (player) {
		case BLACK -> this.cells.set(idx, Cell.BLACK);
		case WHITE -> this.cells.set(idx, Cell.WHITE);
		}

		this.moves.remove(new Move(x, y));

		return true;
	}

	public void swap() {
		this.moves.clear();

		for (int j = 0; j < this.size; j++) {
			for (int i = 0; i < this.size; i++) {
				int idx = j * this.size + i;

				Cell cell = this.cells.get(idx);

				switch (cell) {
				case BLACK -> this.cells.set(idx, Cell.WHITE);
				case WHITE -> this.cells.set(idx, Cell.BLACK);
				case EMPTY -> this.moves.add(new Move(i, j));
				}
			}
		}

		Collections.shuffle(this.moves);
	}

	public Optional<Move> next() {
		if (this.moves.isEmpty()) return Optional.empty();

		return Optional.of(this.moves.remove(this.moves.size() - 1));
	}
}

sealed interface NetMessage {}

record StartMessage(HexPlayer player, int boardSize, int gameSecs, int threadLimit, int memLimitMib) implements NetMessage {}
record MoveMessage(int boardX, int boardY) implements NetMessage {}
record SwapMessage() implements NetMessage {}
record EndMessage(HexPlayer winner) implements NetMessage {}

class Net {
	private final Socket sock;
	private final OutputStream out;
	private final InputStream in;

	public static final int MESSAGE_SIZE = 32;

	public Net(String host, int port) throws IOException {
		this.sock = new Socket(host, port);

		this.out = this.sock.getOutputStream();
		this.in = this.sock.getInputStream();
	}

	public Optional<NetMessage> recvMsg() {
		byte[] buf = new byte[MESSAGE_SIZE];

		int nbytes_recv = 0;

		do {
			int curr;
			try {
				curr = this.in.read(buf, nbytes_recv, buf.length - nbytes_recv);
			} catch (IOException ex) {
				return Optional.empty();
			}

			if (curr <= 0) return Optional.empty();
			nbytes_recv += curr;
		} while (nbytes_recv < buf.length);

		return deserialiseMsg(ByteBuffer.wrap(buf));
	}

	public boolean sendMsg(NetMessage msg) {
		byte[] buf = new byte[MESSAGE_SIZE];

		serialiseMsg(msg, ByteBuffer.wrap(buf));

		try {
			this.out.write(buf);
			this.out.flush();
		} catch (IOException ex) {
			return false;
		}

		return true;
	}

	private static void serialiseMsg(NetMessage msg, ByteBuffer buf) {
		buf.order(ByteOrder.BIG_ENDIAN);

		if (msg instanceof StartMessage start) {
			buf.putInt(HexMessageType.START.value);
			buf.putInt(start.player().value);
			buf.putInt(start.boardSize());
			buf.putInt(start.gameSecs());
			buf.putInt(start.threadLimit());
			buf.putInt(start.memLimitMib());
		} else if (msg instanceof MoveMessage move) {
			buf.putInt(HexMessageType.MOVE.value);
			buf.putInt(move.boardX());
			buf.putInt(move.boardY());
		} else if (msg instanceof SwapMessage swap) {
			buf.putInt(HexMessageType.SWAP.value);
		} else if (msg instanceof EndMessage end) {
			buf.putInt(HexMessageType.END.value);
			buf.putInt(end.winner().value);
		}
	}

	private static Optional<NetMessage> deserialiseMsg(ByteBuffer buf) {
		buf.order(ByteOrder.BIG_ENDIAN);

		int type = buf.getInt();

		if (type == HexMessageType.START.value) {
			HexPlayer player = HexPlayer.fromRaw(buf.getInt());
			int boardSize = buf.getInt();
			int gameSecs = buf.getInt();
			int threadLimit = buf.getInt();
			int memLimitMib = buf.getInt();
			return Optional.of(new StartMessage(player, boardSize, gameSecs, threadLimit, memLimitMib));
		} else if (type == HexMessageType.MOVE.value) {
			int boardX = buf.getInt();
			int boardY = buf.getInt();
			return Optional.of(new MoveMessage(boardX, boardY));
		} else if (type == HexMessageType.SWAP.value) {
			return Optional.of(new SwapMessage());
		} else if (type == HexMessageType.END.value) {
			HexPlayer winner = HexPlayer.fromRaw(buf.getInt());
			return Optional.of(new EndMessage(winner));
		} else {
			return Optional.empty();
		}
	}
}
