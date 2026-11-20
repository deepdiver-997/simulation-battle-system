"""到 sim_server 的一条 TCP 会话（单连接单对局）。

读循环任务持续把线帧解出来放进异步队列；写方向由外部直接调 send()。
断线语义：对端关闭或读写出错 → 队列里放一个 None，recv() 返回 None。
"""

import asyncio
import json
import logging

from . import proto

log = logging.getLogger("sim_client")


class SimClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._queue: asyncio.Queue[dict | None] = asyncio.Queue()
        self._read_task: asyncio.Task | None = None
        self.closed = False
        # 版本指纹（2026-10-01）：连接即 HELLO，None = 老服务端/探针失败（跳过比对）。
        self.fingerprint: dict | None = None

    async def connect(self) -> None:
        self._reader, self._writer = await asyncio.open_connection(self.host, self.port)
        self._read_task = asyncio.create_task(self._read_loop(), name="sim-read-loop")
        await self._handshake()

    async def _handshake(self) -> None:
        """连接即 HELLO：拿引擎指纹。老服务端回 ERROR 帧（不识别 HELLO）→ 指纹置 None，
        绝不因握手失败挡住对局（向后兼容优先，指纹是报警器不是门禁）。"""
        try:
            await self.send(proto.C_HELLO)
            frame = await asyncio.wait_for(self._queue.get(), timeout=5)
            if frame is not None and frame["cmd"] == proto.C_HELLO:
                self.fingerprint = json.loads(frame["payload"].decode("utf-8"))
            elif frame is not None:
                # 老 server 的 "unsupported command" ERROR 帧被这里消费掉——它本来
                # 也只会出现在开局前，语义无害。
                log.info("sim_server 未回 HELLO（cmd=%d）——旧版本服务端，指纹跳过",
                         frame["cmd"])
        except (asyncio.TimeoutError, ValueError, OSError) as exc:
            log.warning("HELLO 握手失败（指纹跳过）：%s", exc)

    async def close(self) -> None:
        self.closed = True
        if self._read_task:
            self._read_task.cancel()
        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except (ConnectionError, OSError):
                pass

    async def send(self, cmd: int, payload: bytes = b"") -> None:
        if not self._writer:
            raise RuntimeError("sim connection not open")
        self._writer.write(proto.build_frame(cmd, 0, payload))
        await self._writer.drain()

    async def recv(self) -> dict | None:
        """取下一条服务端帧；连接断开返回 None。{"cmd": int, "payload": bytes}"""
        if self.closed:
            return None
        return await self._queue.get()

    async def _read_loop(self) -> None:
        try:
            reader = self._reader
            assert reader is not None
            while not self.closed:
                head = await reader.readexactly(proto.HEADER_SIZE)
                total, cmd, _uuid = proto.parse_header(memoryview(head))
                if total < proto.HEADER_SIZE or total > proto.MAX_FRAME:
                    log.warning("非法帧长 total=%d，断开", total)
                    break
                payload = await reader.readexactly(total - proto.HEADER_SIZE)
                await self._queue.put({"cmd": cmd, "payload": bytes(payload)})
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            pass
        except asyncio.CancelledError:
            raise
        finally:
            self.closed = True
            await self._queue.put(None)  # 唤醒 recv() 侧：连接已断
