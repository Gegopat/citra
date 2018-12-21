import socket
import struct
import enum

CURRENT_REQUEST_VERSION = 2
MAX_MEMORY_REQUEST_DATA_SIZE = 32


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
    AreButtonsPressed = 16,
    SetFrameAdvancing = 17,
    AdvanceFrame = 18,
    GetCurrentFrame = 19


CITRA_PORT = 45987


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
    def __init__(self, address='127.0.0.1', port=CITRA_PORT):
        self._socket = socket.socket()
        self._socket.connect((address, port))

    def is_connected(self):
        return self._socket is not None

    def _generate_header(self, request_type, data_size):
        return struct.pack('III', CURRENT_REQUEST_VERSION, request_type, data_size)

    def _recv(self, size):
        data = bytearray()
        # Loop until all expected data is received.
        while len(data) < size:
            buffer = self._socket.recv(size - len(data))
            if not buffer:
                # End of stream was found when unexpected.
                raise EOFError('Couldn\'t receive all expected data!')
            data.extend(buffer)
        return bytes(data)

    def _read_and_validate_header(self, expected_type):
        header = self._recv(4 * 3)
        reply_version, reply_type, reply_data_size = struct.unpack(
            'III', header)
        if reply_version == CURRENT_REQUEST_VERSION and reply_type == expected_type:
            return self._recv(reply_data_size)
        return None

    # Reads memory.
    #   read_address: Address.
    #   read_size: Size.
    # Returns the data read.
    def read_memory(self, read_address, read_size):
        result = bytes()
        while read_size > 0:
            temp_read_size = min(read_size, MAX_MEMORY_REQUEST_DATA_SIZE)
            request_data = struct.pack('II', read_address, temp_read_size)
            request = self._generate_header(
                RequestType.ReadMemory, len(request_data))
            request += request_data
            self._socket.sendall(request)

            reply_data = self._read_and_validate_header(RequestType.ReadMemory)

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
            temp_write_size = min(write_size, MAX_MEMORY_REQUEST_DATA_SIZE - 8)
            request_data = struct.pack('II', write_address, temp_write_size)
            request_data += write_contents[:temp_write_size]
            request = self._generate_header(
                RequestType.WriteMemory, len(request_data))
            request += request_data
            self._socket.sendall(request)

            reply_data = self._read_and_validate_header(
                RequestType.WriteMemory)

            if reply_data is not None:
                write_address += temp_write_size
                write_size -= temp_write_size
                write_contents = write_contents[temp_write_size:]
            else:
                return False
        return True

    # Sets the pad state.
    #   pad_state: Raw pressed buttons (Use the Button enum)
    def set_pad_state(self, pad_state):
        request_data = struct.pack('IIi', 0, 0, pad_state)
        request = self._generate_header(
            RequestType.PadState, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the touch state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    #   valid: This is written to HID Shared Memory.
    def set_touch_state(self, x, y, valid):
        request_data = struct.pack('IIhh?', 0, 0, x, y, valid)
        request = self._generate_header(
            RequestType.TouchState, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the motion state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    #   z: Z-coordinate.
    #   roll: Roll.
    #   pitch: Pitch.
    #   yaw: Yaw.
    def set_motion_state(self, x, y, z, roll, pitch, yaw):
        request_data = struct.pack('IIhhhhhh', 0, 0, x, y, z, roll, pitch, yaw)
        request = self._generate_header(
            RequestType.MotionState, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the Circle Pad state.
    #   x: X-coordinate.
    #   y: Y-coordinate.
    def set_circle_state(self, x, y):
        request_data = struct.pack('IIhh', 0, 0, x, y)
        request = self._generate_header(
            RequestType.CircleState, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets resolution.
    #   resolution: Resolution.
    def set_resolution(self, resolution):
        if resolution == 0:
            raise Exception('Invalid resolution')
        request_data = struct.pack('IIh', 0, 0, resolution)
        request = self._generate_header(
            RequestType.SetResolution, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the program.
    #   path: Full path to the program.
    def set_program(self, path):
        request_data = struct.pack('II', 0, 0)
        request_data += path.encode()
        request = self._generate_header(
            RequestType.SetProgram, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets override controls.
    #   pad: Use overriden pad state.
    #   touch: Use overriden touch state.
    #   motion: Use overriden motion state.
    #   circle: Use overriden Circle Pad state.
    def set_override_controls(self, pad, touch, motion, circle):
        request_data = struct.pack('II????', 0, 0, pad, touch, motion, circle)
        request = self._generate_header(
            RequestType.SetOverrideControls, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Resumes the program.
    def resume(self):
        request_data = struct.pack('II', 0, 0)
        request = self._generate_header(
            RequestType.Resume, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Pauses the program.
    def pause(self):
        request_data = struct.pack('II', 0, 0)
        request = self._generate_header(
            RequestType.Pause, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Restarts the program.
    def restart(self):
        request_data = struct.pack('II', 0, 0)
        request = self._generate_header(
            RequestType.Restart, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the speed limit.
    #   speed_limit: Speed limit.
    def set_speed_limit(self, speed_limit):
        request_data = struct.pack('IIh', 0, 0, speed_limit)
        request = self._generate_header(
            RequestType.SetSpeedLimit, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the background color.
    #   r: Red.
    #   g: Green.
    #   b: Blue.
    def set_background_color(self, r, g, b):
        request_data = struct.pack('IIfff', 0, 0, r, g, b)
        request = self._generate_header(
            RequestType.SetBackgroundColor, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Sets the screen refresh rate.
    #   rate: Screen refresh rate.
    def set_screen_refresh_rate(self, rate):
        if rate < 1 or rate > 1000:
            raise Exception('Invalid screen refresh rate')
        request_data = struct.pack('IIf', 0, 0, rate)
        request = self._generate_header(
            RequestType.SetScreenRefreshRate, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Gets whether buttons are currently pressed.
    #   buttons: Buttons (use Button enum).
    # Returns: True if the buttons are pressed or False if the buttons aren't pressed.
    def are_buttons_pressed(self, buttons):
        request_data = struct.pack('IIi', 0, 0, buttons)
        request = self._generate_header(
            RequestType.AreButtonsPressed, len(request_data))
        request += request_data
        self._socket.sendall(request)
        return self._read_and_validate_header(RequestType.AreButtonsPressed)[0] == 1

    # Sets whether frame advancing is enabled.
    #   enabled: True to enable, False to disable.
    def set_frame_advancing(self, enabled):
        request_data = struct.pack('II?', 0, 0, enabled)
        request = self._generate_header(
            RequestType.SetFrameAdvancing, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Advances a frame. Enables frame advancing if not enabled.
    def advance_frame(self):
        request_data = struct.pack('II', 0, 0)
        request = self._generate_header(
            RequestType.AdvanceFrame, len(request_data))
        request += request_data
        self._socket.sendall(request)

    # Gets the current frame.
    def get_current_frame(self):
        request_data = struct.pack('II', 0, 0)
        request = self._generate_header(
            RequestType.GetCurrentFrame, len(request_data))
        request += request_data
        self._socket.sendall(request)
        return self._read_and_validate_header(RequestType.GetCurrentFrame)
