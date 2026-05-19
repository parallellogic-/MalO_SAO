class BMPReader(object):
	def __init__(self, filename):
		self._filename = filename
		if(".bmp" in filename):
			#import malo_c_module
			#buff=malo_c_module.buffer()
			buff=[0] * (128 * 64)
			buff=self._read_img_data3(buff);
			for row in range(128):
				for col in range(64):
					print("." if buff[row*64+col] else " ",end='')
				print()
			for row in range(128):
				for col in range(64):
					print(hex(buff[row*64+col]),end=',')
				print()
		else:
			import malo_c_module
			buff=malo_c_module.buffer()
			with open(filename, "rb") as f:
				mv = memoryview(buff)[0:(128*64)]
				n = f.readinto(mv)  # reads directly into the buffer
				#print(f"Loaded {n} bytes into RAM buffer")

	def get_pixels(self):
		"""
		Returns a multi-dimensional array of the RGB values of each pixel in the
		image, arranged by rows and columns from the top-left.  Access any pixel
		by its location, eg:

		pixels = BMPReader(filename).get_pixels()
		top_left_px = pixels[0][0] # [255, 0, 0]
		bottom_right_px = pixels[8][8] # [0, 255, 255]
		"""
		pixel_grid = []
		pixel_data = list(self._pixel_data) # So we're working on a copy

		for x in range(self.width):
			col = []
			for y in range(self.height):
				r = pixel_data.pop()
				g = pixel_data.pop()
				b = pixel_data.pop()
				col.append((r, g, b))
			col.reverse()
			pixel_grid.append(col)

		return pixel_grid

	def _read_img_data(self):
		def lebytes_to_int(bytes):
			n = 0x00
			while len(bytes) > 0:
				n <<= 8
				n |= bytes.pop()
			return int(n)
		with open(self._filename, 'rb') as f:
			img_bytes = list(bytearray(f.read()))
		# Before we proceed, we need to ensure certain conditions are met
		assert img_bytes[0:2] == [66, 77], "Not a valid BMP file"
		assert lebytes_to_int(img_bytes[30:34]) == 0, \
			"Compression is not supported"
		assert lebytes_to_int(img_bytes[28:30]) == 24, \
			"Only 24-bit colour depth is supported"

		start_pos = lebytes_to_int(img_bytes[10:14])
		end_pos = start_pos + lebytes_to_int(img_bytes[34:38])
		print(f"start_pos: {start_pos}")
		print(f"end_pos: {end_pos}")

		self.width = lebytes_to_int(img_bytes[18:22])
		self.height = lebytes_to_int(img_bytes[22:26])
		print(f"width: {self.width}")
		print(f"height: {self.height}")

		self._pixel_data = img_bytes[start_pos:end_pos]
		
	def _read_img_data2(self):
		lookup={
				0:66,
				1:77,
				28:24,
				29:0,
				30:0,
				31:0,
				32:0,
				33:0
			}
		start_pos=0x00000000
		end_pos=0x00000000
		self.width=0x00000000
		self.height=0x00000000
		in_value=0x0000
		rgb_index=0x00
		out_value=0x00
		import malo_c_module
		buff=malo_c_module.buffer()
		in_pixel_index=0
		out_byte_index=0
		line_index=-1
		with open(self._filename, 'rb') as file_handle:
			#for line_index,line_byte in enumerate(file_handle):
			while True:
				b = file_handle.read(1)  # returns a bytes object of length 1
				line_index+=1
				if not b:
					break
				line_byte = b[0]  # convert to integer
				#print("A")
				if(line_index in lookup): assert line_byte==lookup[line_index],f"Issue at byte_index {line_index}, {int(line_byte)}=/={lookup[line_index]}"
				if(10<=line_index<14): start_pos=(start_pos<<8) | line_byte
				if(34<=line_index<38): end_pos=(end_pos<<8) | line_byte
				if(18<=line_index<22): self.width=(self.width<<8) | line_byte
				if(22<=line_index<26): self.height=(self.height<<8) | line_byte
				#print(f"{start_pos}<{line_index}<{end_pos}")
				if(line_index==38):
					start_pos=((start_pos&0x000000FF)<<24)|((start_pos&0x0000FF00)<<8)|((start_pos&0x00FF0000)>>8)|((start_pos&0xFF000000)>>24)
					end_pos=((end_pos&0x000000FF)<<24)|((end_pos&0x0000FF00)<<8)|((end_pos&0x00FF0000)>>8)|((end_pos&0xFF000000)>>24)
					self.width=((self.width&0x000000FF)<<24)|((self.width&0x0000FF00)<<8)|((self.width&0x00FF0000)>>8)|((self.width&0xFF000000)>>24)
					self.height=((self.height&0x000000FF)<<24)|((self.height&0x0000FF00)<<8)|((self.height&0x00FF0000)>>8)|((self.height&0xFF000000)>>24)
					print(f"start_pos: {start_pos}")
					print(f"end_pos: {end_pos}")
					print(f"width: {self.width}")
					print(f"height: {self.height}")
				if(line_index>=38 and start_pos<=line_index<end_pos):
					in_value+=line_byte
					rgb_index+=1
					#print("D")
					if(rgb_index==2):
						nibble=in_value//(3*16) #3->1 colors and 256->16 states
						in_value=0
						rgb_index=0
						out_value=(out_value<<4)|nibble
						#print("E")
						if(in_pixel_index%2==1): #only populate out pixels every-other nibble, PRECON: image is even pixel count wide
							buff[out_byte_index]=out_value
							#if(out_byte_index<1000):
							#	buff[out_byte_index]=0xFF
								#print("HERE")
							out_byte_index+=1
							out_value=0x00
							in_pixel_index=0
						else:
							in_pixel_index=1
				#if(line_index<100): print("%d"%)
		for row in range(128):
			for col in range(64):
				print("." if buff[row*64+col] else " ",end='')
			print()
			
	def _read_img_data3(self,buff):
		lookup={
				0:66,
				1:77,
				28:24,
				29:0,
				30:0,
				31:0,
				32:0,
				33:0
			}
		start_pos=0x00000000
		end_pos=0x00000000
		self.width=0x00000000
		self.height=0x00000000
		in_value=0x0000
		rgb_index=0x00
		out_value=0x00
		in_pixel_index=0
		out_byte_index=0
		line_index=-1
		with open(self._filename, 'rb') as file_handle:
			#for line_index,line_byte in enumerate(file_handle):
			while True:
				b = file_handle.read(1)  # returns a bytes object of length 1
				line_index+=1
				if not b:
					break
				line_byte = b[0]  # convert to integer
				#print("A")
				if(line_index in lookup): assert line_byte==lookup[line_index],f"Issue at byte_index {line_index}, {int(line_byte)}=/={lookup[line_index]}"
				if(10<=line_index<14): start_pos=(start_pos<<8) | line_byte
				if(34<=line_index<38): end_pos=(end_pos<<8) | line_byte
				if(18<=line_index<22): self.width=(self.width<<8) | line_byte
				if(22<=line_index<26): self.height=(self.height<<8) | line_byte
				#print(f"{start_pos}<{line_index}<{end_pos}")
				if(line_index==37):
					start_pos=((start_pos&0x000000FF)<<24)|((start_pos&0x0000FF00)<<8)|((start_pos&0x00FF0000)>>8)|((start_pos&0xFF000000)>>24)
					end_pos=((end_pos&0x000000FF)<<24)|((end_pos&0x0000FF00)<<8)|((end_pos&0x00FF0000)>>8)|((end_pos&0xFF000000)>>24)
					self.width=((self.width&0x000000FF)<<24)|((self.width&0x0000FF00)<<8)|((self.width&0x00FF0000)>>8)|((self.width&0xFF000000)>>24)
					self.height=((self.height&0x000000FF)<<24)|((self.height&0x0000FF00)<<8)|((self.height&0x00FF0000)>>8)|((self.height&0xFF000000)>>24)
					print(f"start_pos: {start_pos}")
					print(f"end_pos: {end_pos}")
					print(f"width: {self.width}")
					print(f"height: {self.height}")
					break
			file_handle.read(start_pos-38) #skip a few
			line_index=0
			while True:
				b = file_handle.read(6)  # read rgb*2
				if(line_index<self.width*self.height//2):
					value_high=(b[0]+b[1]+b[2])//(3*16)
					value_low=(b[3]+b[4]+b[5])//(3*16) #only entry with data
					buff[line_index]=(value_high<<4)+value_low #re-pack two rgb as one byte of grayscale
				else: break
				line_index+=1
		#for row in range(128):
		#	for col in range(64):
		#		print("." if buff[row*64+col] else " ",end='')
		#	print()
		return buff

if __name__=="__main__":
	print("START")
	import sys
	BMPReader(sys.argv[1])
