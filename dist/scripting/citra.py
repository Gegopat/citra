import zmq
import struct
import random
import enum

CURRENT_REQUEST_VERSION = 1
MAX_REQUEST_DATA_SIZE = 32


class RequestType(enum.IntEnum):
    ReadMemory = 1,
    WriteMemory = 2,
    PadState = 3,
    TouchState = 4,
    MotionState = 5,
    CircleState = 6,
    SetResolution = 7,
    SetProgram = 8,
    SetOverrideControls = 9,
    Pause = 10,
    Resume = 11,
    Restart = 12,
    SetSpeedLimit = 13,
    SetBackgroundColor = 14,
    SetScreenRefreshRate = 15,
    IsButtonPressed = 16,
    SetFrameAdvancing = 17,
    AdvanceFrame = 18,
    GetCurrentFrame = 19


CITRA_PORT = "45987"


def BIT(n):  # https://github.com/smealum/ctrulib/blob/master/libctru/include/3ds/types.h#L46
    return (1 << n)


class Button(enum.IntEnum):
    A = BIT(0),             # A
    B = BIT(1),             # B
    Select = BIT(2),        # Select
    Start = BIT(3),         # Start
    DRight = BIT(4),        # D-Pad Right
    DLeft = BIT(5),         # D-Pad Left
    DUp = BIT(6),           # D-Pad Up
    DDown = BIT(7),         # D-Pad Down
    R = BIT(8),             # R
    L = BIT(9),             # L
    X = BIT(10),            # X
    Y = BIT(11),            # Y
    CircleRight = BIT(28),  # Circle Pad Right
    CircleLeft = BIT(29),   # Circle Pad Left
    CircleUp = BIT(30),     # Circle Pad Up
    CircleDown = BIT(31)    # Circle Pad Down


class Citra:
    def __init__(self, address="127.0.0.1", port=CITRA_PORT):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect("tcp://" + address + ":" + port)

    def is_connected(self):
        return self.socket is not None

    def _generate_header(self, request_type, data_size):
        request_id = random.getrandbits(32)
        return (struct.pack("IIII", CURRENT_REQUEST_VERSION, request_id, request_type, data_size), request_id)

    def _read_and_validate_header(self, raw_reply, expected_id, expected_type):
        reply_version, reply_id, reply_type, reply_data_size = struct.unpack(
            "IIII", raw_reply[:4*4])
        if (CURRENT_REQUEST_VERSION == reply_version and
            expected_id == reply_id and
            expected_type == reply_type and
                reply_data_size == len(raw_reply[4*4:])):
            return raw_reply[4*4:]
        return None

    # Reads memory.
    #   read_address: Address.
    #   read_size: Size.
    # Returns the data read.
    def read_memory(self, read_address, read_size):
        result = bytes()
        while read_size > 0:
            temp_read_size = min(read_size, MAX_REQUEST_DATA_SIZE)
            request_data = struct.pack("II", read_address, temp_read_size)
            request, request_id = self._generate_header(
                RequestType.ReadMemory, len(request_data))
            request += request_data
            self.socket.send(request)

            raw_reply = self.socket.recv()
            reply_data = self._read_and_validate_header(
                raw_reply, request_id, RequestType.ReadMemory)

            if reply_data:
                result += reply_data
                read_size -= len(reply_data)
                read_address += len(reply_data)
            else:
                return None

        return result

    # Writes memory.
    #   write_address: Address.
    #   write_contents: Contents to write.
    def write_memory(self, write_address, write_contents):
        write_size = len(write_contents)
        while write_size > 0:
            temp_write_size = min(write_size, MAX_REQUEST_DATA_SIZE - 8)
            request_data = struct.pack("II", write_address, temp_write_size)
            request_data += write_contents[:temp_write_size]
            request, request_id = self._generate_header(
                RequestType.WriteMemory, len(request_data))
            request += request_data
            self.socket.send(request)

            raw_reply = self.socket.recv()
            reply_data = self._read_and_validate_header(
                raw_reply, request_id, RequestType.WriteMemory)

            if None != reply_data:
                write_address += temp_write_size
                write_size -= temp_write_size
                write_contents = write_contents[temp_write_size:]
            else:
                return False
        return True

    # Sets the pad state.
    #   pad_state: Raw pressed buttons (Use Key enum)
    def set_pad_state(self, pad_state):
        request_data = struct.pack("IIi", 0, 0, pad_state)
        request, request_id = self._generate_header(
            RequestType.PadState, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the touch state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    #   valid: This is written to HID Shared Memory.
    def set_touch_state(self, x, y, valid):
        request_data = struct.pack("IIhh?", 0, 0, x, y, valid)
        request, request_id = self._generate_header(
            RequestType.TouchState, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the motion state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    #   z: Z-coordinate.
    #   roll: Roll.
    #   pitch: Pitch.
    #   yaw: Yaw.
    def set_motion_state(self, x, y, z, roll, pitch, yaw):
        request_data = struct.pack("IIhhhhhh", 0, 0, x, y, z, roll, pitch, yaw)
        request, request_id = self._generate_header(
            RequestType.MotionState, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the Circle Pad state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    def set_circle_state(self, x, y):
        request_data = struct.pack("IIhh", 0, 0, x, y)
        request, request_id = self._generate_header(
            RequestType.CircleState, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets resolution.
    #   resolution: Resolution.
    def set_resolution(self, resolution):
        if resolution == 0:
            raise Exception("Invalid resolution")
        request_data = struct.pack("IIh", 0, 0, resolution)
        request, request_id = self._generate_header(
            RequestType.SetResolution, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the program.
    #   path: Full path to the program.
    def set_program(self, path):
        request_data = struct.pack("II", 0, 0)
        request_data += str.encode(path)
        request, request_id = self._generate_header(
            RequestType.SetProgram, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets override controls.
    #   pad: Use overriden pad state.
    #   touch: Use overriden touch state.
    #   motion: Use overriden motion state.
    #   circle: Use overriden Circle Pad state.
    def set_override_controls(self, pad, touch, motion, circle):
        request_data = struct.pack("II????", 0, 0, pad, touch, motion, circle)
        request, request_id = self._generate_header(
            RequestType.SetOverrideControls, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Pause the application.
    def pause(self):
        request_data = struct.pack("II", 0, 0)
        request, request_id = self._generate_header(
            RequestType.Pause, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Resume the application.
    def resume(self):
        request_data = struct.pack("II", 0, 0)
        request, request_id = self._generate_header(
            RequestType.Resume, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Restarts the application.
    def restart(self):
        request_data = struct.pack("II", 0, 0)
        request, request_id = self._generate_header(
            RequestType.Restart, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the speed limit.
    #   speed_limit: Speed limit.
    def set_speed_limit(self, speed_limit):
        request_data = struct.pack("IIh", 0, 0, speed_limit)
        request, request_id = self._generate_header(
            RequestType.SetSpeedLimit, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the background color.
    #   r: Red.
    #   g: Gree
    #   b: Blue.
    def set_background_color(self, r, g, b):
        request_data = struct.pack("IIfff", 0, 0, r, g, b)
        request, request_id = self._generate_header(
            RequestType.SetBackgroundColor, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Sets the screen refresh rate.
    #   rate: Screen refresh rate.
    def set_screen_refresh_rate(self, rate):
        if rate == 0:
            raise Exception("Invalid screen refresh rate")
        request_data = struct.pack("IIi", 0, 0, rate)
        request, request_id = self._generate_header(
            RequestType.SetScreenRefreshRate, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Gets whether a button is currently pressed.
    #   button: Button (use Button enum).
    # Returns: True if the button is pressed or False if the button is not pressed.
    def is_button_pressed(self, button):
        request_data = struct.pack("IIi", 0, 0, button)
        request, request_id = self._generate_header(
            RequestType.IsButtonPressed, len(request_data))
        request += request_data
        self.socket.send(request)
        raw_reply = self.socket.recv()
        data = self._read_and_validate_header(
            raw_reply, request_id, RequestType.IsButtonPressed)
        return data[0] == 1

    # Sets whether frame advancing is enabled.
    #   enabled: True to enable, False to disable.
    def set_frame_advancing(self, enable):
        request_data = struct.pack("II?", 0, 0, enabled)
        request, request_id = self._generate_header(
            RequestType.SetFrameAdvancing, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Advances a frame. Enables frame advancing if not enabled.
    def advance_frame(self):
        request_data = struct.pack("II", 0, 0)
        request, request_id = self._generate_header(
            RequestType.AdvanceFrame, len(request_data))
        request += request_data
        self.socket.send(request)
        self.socket.recv()

    # Gets the current frame.
    def get_current_frame(self):
        request_data = struct.pack("II", 0, 0)
        request, request_id = self._generate_header(
            RequestType.GetCurrentFrame, len(request_data))
        request += request_data
        self.socket.send(request)
        raw_reply = self.socket.recv()
        data = self._read_and_validate_header(
            raw_reply, request_id, RequestType.GetCurrentFrame)
        return data
