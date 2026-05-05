import struct
from PIL import Image


class TextAndSensorDecoder:
    def __init__(self):
        # I (uint32/4 bytes), h (int16/2 bytes), B (uint8/1 byte), i (int32/4 bytes)
        # Using '<' for little-endian standard size alignment
        self.sensor_format = '<I h h I h i i B h h h'
        self.expected_size = struct.calcsize(self.sensor_format)

    def decodeASCII(self, payload_bytes):
        # Decodifica texto mandado pela linha Serial.
        try:
            # decodifica bytes para string e strip null terminator ou newlines
            return payload_bytes.decode('utf-8', errors='ignore').strip('\x00\r\n')
        except Exception as e:
            return f"ERRO AO DECODIFICAR: {e}"

    def decode_sensor_struct(self, payload_bytes):
        if len(payload_bytes) < self.expected_size:
            # FIX: Returning a dictionary instead of a set
            return {"error": f"PAYLOAD MUITO PEQUENO. ESPERADO {self.expected_size}, RECEBIDO {len(payload_bytes)}"}

        data = struct.unpack(self.sensor_format, payload_bytes[:self.expected_size])

        # mapa de ler Tuple
        return {
            "seconds": data[0],
            "temperatura": data[1],
            "umidade": data[2],
            "pressao": data[3],
            "altitude": data[4],
            "latitude": data[5],
            "longitude": data[6],
            "sats": data[7],
            "accelX": data[8],
            "accelY": data[9],
            "accelZ": data[10]
        }


class ImageDecoder:
    def __init__(self, output_width=80, output_height=60):
        self.width = output_width
        self.height = output_height
        self.total_chunks = 0
        self.received_chunks = {}

    def add_chunk(self, payload_bytes):
        # Basic bounds check to prevent IndexError on tiny/empty payloads
        if len(payload_bytes) < 3:
            return False

        chunk_index = payload_bytes[0]
        total_chunks = payload_bytes[1]
        chunk_len = payload_bytes[2]

        rle_data = payload_bytes[3: 3 + chunk_len]

        self.total_chunks = total_chunks
        self.received_chunks[chunk_index] = rle_data

        if len(self.received_chunks) == self.total_chunks:
            # FIX: matched the method name exactly
            return self._build_and_save_image()
        return False

    def _build_and_save_image(self):
        full_rle_data = bytearray()

        for i in range(self.total_chunks):
            # Safe access in case a chunk index was somehow missed
            if i in self.received_chunks:
                full_rle_data.extend(self.received_chunks[i])

        pixel_data = []

        # SAFEGUARD: Ensure we have an even number of bytes for RLE decoding
        rle_length = len(full_rle_data)
        if rle_length % 2 != 0:
            print("WARNING: RLE data length is odd, truncating the last byte to avoid crash.")
            rle_length -= 1

        for i in range(0, rle_length, 2):
            count = full_rle_data[i]
            value = full_rle_data[i + 1]

            color = 255 if value == 1 else 0
            pixel_data.extend([color] * count)

        # SAFEGUARD: Ensure pixel data matches image dimensions
        expected_pixels = self.width * self.height
        if len(pixel_data) > expected_pixels:
            pixel_data = pixel_data[:expected_pixels]  # Truncate if too long
        elif len(pixel_data) < expected_pixels:
            # Pad with black if too short (due to dropped packets)
            pixel_data.extend([0] * (expected_pixels - len(pixel_data)))

        img = Image.new('L', (self.width, self.height))
        img.putdata(pixel_data)

        # FIX: Changed .ping to .png
        filename = "telemetry_image.png"
        img.save(filename)

        # Reset for the next image transmission
        self.received_chunks = {}
        return True
