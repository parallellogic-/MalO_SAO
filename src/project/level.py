

class Level:
	def __init__(self,pause_menu):
		self.pause_menu=None
		self.is_running=False
		self.frame_index=0
		self.frame_index_unpaused=0
		
	def dispose(self):
		self.is_running=False
		self.pause_menu.dispose()
		
	#running game loop
	def run(self):
		self.is_running=True
		clock = pygame.time.Clock()
		while self.is_running:
			if(self.pause_menu.is_paused):
				self.pause_menu.draw()
			else:
				self.draw()
				self.frame_index_unpaused+=1
			pygame.display.update()
			clock.tick(60)
			self.frame_index+=1
		
	#process a single frame (16.6ms max)
	def draw(self):
		pass
		
if __name__=="__main__":
	pygame.init()
	level=Level()
	level.run()