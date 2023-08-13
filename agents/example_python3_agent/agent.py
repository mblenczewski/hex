#!/usr/bin/env python3

from __future__ import annotations

import enum
import random
import socket
import struct
import sys


class PlayerType(enum.Enum):
    PLAYER_BLACK    = 0
    PLAYER_WHITE    = 1

    def __str__(self) -> str:
        match self:
            case self.PLAYER_BLACK: return 'black'
            case self.PLAYER_WHITE: return 'white'


class MsgType(enum.Enum):
    MSG_START       = 0
    MSG_MOVE        = 1
    MSG_SWAP        = 2
    MSG_END         = 3


class MsgStartData:
    def __init__(self, player: int, board_size: int, game_secs: int, thread_limit: int, mem_limit_mib: int):
        self.player = PlayerType(player)
        self.board_size = board_size
        self.game_secs = game_secs
        self.thread_limit = thread_limit
        self.mem_limit_mib = mem_limit_mib

    def as_tuple(self) -> tuple[PlayerType, int, int, int, int]:
        return self.player, self.board_size, self.game_secs, self.thread_limit, self.mem_limit_mib


class MsgMoveData:
    def __init__(self, board_x: int, board_y: int):
        self.board_x = board_x
        self.board_y = board_y

    def __repr__(self) -> str:
        return f'move: ({self.board_x}, {self.board_y})'

    def as_tuple(self) -> tuple[int, int]:
        return self.board_x, self.board_y


class MsgSwapData:
    def __init__(self):
        pass

    def __repr__(self) -> str:
        return f'swap'


class MsgEndData:
    def __init__(self, winner: int):
        self.winner = PlayerType(winner)

    def __repr__(self) -> str:
        return f'end: {self.winner}'

    def as_tuple(self) -> tuple[PlayerType]:
        return self.winner,


MsgData = MsgStartData | MsgMoveData | MsgSwapData | MsgEndData


class Msg:
    def __init__(self, typ: MsgType, dat: MsgData):
        self.typ = typ
        self.dat = dat

    def __repr__(self) -> str:
        return f'msg: {self.typ}, {self.dat}'

    @classmethod
    def size(cls) -> int:
        return 32

    def serialise_into(self, buffer: memoryview) -> int:
        assert Msg.size() <= len(buffer)

        match self.typ:
            case MsgType.MSG_START: # this message type is never sent by the client
                pass

            case MsgType.MSG_MOVE:
                struct.pack_into('!III', buffer, 0,
                                 self.typ.value,
                                 self.dat.board_x,
                                 self.dat.board_y)

            case MsgType.MSG_SWAP:
                struct.pack_into('!I', buffer, 0, self.typ.value)

            case MsgType.MSG_END: # this message type is never sent by the client
                pass

        return Msg.size()

    @classmethod
    def deserialise_from(cls, buffer: memoryview) -> Msg:
        assert cls.size() <= len(buffer)

        raw_typ, = struct.unpack_from('!I', buffer, 0)
        typ =  MsgType(raw_typ)

        match typ:
            case MsgType.MSG_START: dat = MsgStartData(*struct.unpack_from('!IIIII', buffer, 4))
            case MsgType.MSG_MOVE:  dat = MsgMoveData(*struct.unpack_from('!II', buffer, 4))
            case MsgType.MSG_SWAP:  dat = MsgSwapData()
            case MsgType.MSG_END:   dat = MsgEndData(*struct.unpack_from('!I', buffer, 4))

        return Msg(typ, dat)


def recv_msg(sock: socket.socket, *, expected_msg_types: list[MsgType]) -> Msg:
    buffer = bytearray(Msg.size())

    def recv_all_bytes(sock: socket.socket, buf: memoryview, sz: int) -> int:
        total = 0
        while total < sz:
            curr = sock.recv_into(buf[total:], sz - total)
            if curr == 0: return total
            total += curr

        return total

    recv_all_bytes(sock, memoryview(buffer), len(buffer))

    return Msg.deserialise_from(buffer)


def send_msg(sock: socket.socket, msg: Msg) -> None:
    buffer = bytearray(Msg.size())

    def send_all_bytes(sock: socket.socket, buf: memoryview, sz: int) -> int:
        total = 0
        while total < sz:
            curr = sock.send(buf[total:], sz - total)
            if curr == 0: return total
            total += curr

        return total

    msg.serialise_into(buffer)

    send_all_bytes(sock, memoryview(buffer), len(buffer))


class Board:
    class Cell(enum.Enum):
        BLACK       = PlayerType.PLAYER_BLACK.value
        WHITE       = PlayerType.PLAYER_WHITE.value
        EMPTY       = enum.auto()

    def __init__(self, board_size: int):
        self.board_size = board_size
        self.board = [self.Cell.EMPTY for _ in range(board_size * board_size)]
        self.remaining_moves = [(i, j) for i in range(board_size) for j in range(board_size)]
        random.shuffle(self.remaining_moves)

    def swap(self) -> None:
        self.remaining_moves = []
        for j in range(self.board_size):
            for i in range(self.board_size):
                match self.board[j * self.board_size + i]:
                    case self.Cell.BLACK:
                        self.board[j * self.board_size + i] = self.Cell.WHITE

                    case self.Cell.WHITE:
                        self.board[j * self.board_size + i] = self.Cell.BLACK

                    case self.Cell.EMPTY:
                        self.remaining_moves.append((i, j))

        random.shuffle(self.remaining_moves)

    def play(self, player: PlayerType, px: int, py: int) -> bool:
        old = self.board[py * self.board_size + px]

        if old != self.Cell.EMPTY:
            return False

        new = None
        match player:
            case PlayerType.PLAYER_BLACK: new = self.Cell.BLACK
            case PlayerType.PLAYER_WHITE: new = self.Cell.WHITE

        self.board[py * self.board_size + px] = new

        for idx, t in enumerate(self.remaining_moves):
            if t[0] == px and t[1] == py:
                self.remaining_moves.pop(idx)
                break

        return True

    def get_next_move(self) -> tuple[int, int]:
        return self.remaining_moves.pop(0)


class GameState(enum.Enum):
    START           = enum.auto()
    RECV            = enum.auto()
    SEND            = enum.auto()
    END             = enum.auto()


def main() -> None:
    if len(sys.argv) < 3:
        print(f'Usage: {sys.argv[0]} <host> <port>', file=sys.stderr)
        quit(1)

    host, port, *args = sys.argv[1:]
    with socket.create_connection((host, port)) as sock:
        state = GameState.START

        player = None
        game_secs = None # unused
        thread_limit = None # unused
        mem_limit_mib = None # unused

        board = None
        other_player = None
        winner = None

        first_round = True
        game_is_over = False
        while not game_is_over:
            match state:
                case GameState.START:
                    msg = recv_msg(sock, expected_msg_types=[MsgType.MSG_START])
                    player, board_size, game_secs, thread_limit, mem_limit_mib = msg.dat.as_tuple()

                    board = Board(board_size)

                    print(f'[{player}] Started game: {board_size}x{board_size}, {game_secs} secs, {thread_limit} threads, {mem_limit_mib} MiB')

                    if player == PlayerType.PLAYER_BLACK:
                        other_player = PlayerType.PLAYER_WHITE
                        state = GameState.SEND

                    elif player == PlayerType.PLAYER_WHITE:
                        other_player = PlayerType.PLAYER_BLACK
                        state = GameState.RECV

                case GameState.RECV:
                    msg = recv_msg(sock, expected_msg_types=[MsgType.MSG_MOVE, MsgType.MSG_SWAP, MsgType.MSG_END])

                    if msg.typ == MsgType.MSG_MOVE:
                        board_x, board_y = msg.dat.as_tuple()
                        board.play(other_player, board_x, board_y)

                        if first_round and random.choice([True, False]):
                            board.swap()

                            msg = Msg(MsgType.MSG_SWAP, MsgSwapData())
                            send_msg(sock, msg)

                            state = GameState.RECV

                        else:
                            state = GameState.SEND

                    elif msg.typ == MsgType.MSG_SWAP:
                        board.swap()

                        state = GameState.SEND

                    elif msg.typ == MsgType.MSG_END:
                        winner, = msg.dat.as_tuple()

                        state = GameState.END

                    first_round = False

                case GameState.SEND:
                    board_x, board_y = board.get_next_move()
                    board.play(player, board_x, board_y)

                    msg = Msg(MsgType.MSG_MOVE, MsgMoveData(board_x, board_y))

                    send_msg(sock, msg)

                    state = GameState.RECV

                    first_round = False

                case GameState.END:
                    print(f'[{player}] Player {winner} has won the game')
                    break

                case _:
                    print(f'[{player}] Unknown state encountered: {state}')
                    break


if __name__ == '__main__':
    main()

