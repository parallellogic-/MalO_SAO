#make me a platformer pygame py code with pause menu. allow for coyote time when running or jumping off platforms, allow for edge scrolling. add 3 keys that need to be collected to unlock doors (doors have collision detection: unable to open without key) . add enemy.  add 3 game elements that are difficult to surpass on the first try, but are easy to pass after understanding them (ex ref crash bandicoot levels).  for moving platforms, player needs to move with platform when it is moving.  bouncy platforms.  show keys collected as icons in top-left of screen

import pygame
import sys

pygame.init()

# =========================
# CONFIG
# =========================
WIDTH, HEIGHT = 960, 540
FPS = 60
GRAVITY = 0.6
COYOTE_TIME = 0.15  # seconds

screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Platformer Challenge")

clock = pygame.time.Clock()
font = pygame.font.SysFont(None, 36)

# =========================
# PLAYER
# =========================
class Player(pygame.sprite.Sprite):
    def __init__(self, x, y):
        super().__init__()
        self.image = pygame.Surface((40, 60))
        self.image.fill((50, 200, 255))
        self.rect = self.image.get_rect(topleft=(x, y))
        self.vel = pygame.Vector2(0, 0)
        self.speed = 5
        self.jump_power = -14
        self.on_ground = False
        self.coyote_timer = 0

    def update(self, platforms):
        keys = pygame.key.get_pressed()

        self.vel.x = 0
        if keys[pygame.K_a]:
            self.vel.x = -self.speed
        if keys[pygame.K_d]:
            self.vel.x = self.speed

        # Coyote time
        if self.on_ground:
            self.coyote_timer = COYOTE_TIME
        else:
            self.coyote_timer -= 1/FPS

        if keys[pygame.K_SPACE]:
            if self.on_ground or self.coyote_timer > 0:
                self.vel.y = self.jump_power
                self.coyote_timer = 0

        self.vel.y += GRAVITY
        self.rect.x += self.vel.x
        self.collide(platforms, 'x')

        self.rect.y += self.vel.y
        self.on_ground = False
        self.collide(platforms, 'y')

    def collide(self, platforms, direction):
        for p in platforms:
            if self.rect.colliderect(p.rect):
                if direction == 'x':
                    if self.vel.x > 0:
                        self.rect.right = p.rect.left
                    if self.vel.x < 0:
                        self.rect.left = p.rect.right
                if direction == 'y':
                    if self.vel.y > 0:
                        self.rect.bottom = p.rect.top
                        self.vel.y = 0
                        self.on_ground = True
                    if self.vel.y < 0:
                        self.rect.top = p.rect.bottom
                        self.vel.y = 0

# =========================
# PLATFORM TYPES
# =========================
class Platform:
    def __init__(self, x, y, w, h):
        self.rect = pygame.Rect(x, y, w, h)
        self.color = (100, 100, 100)

    def update(self):
        pass

    def draw(self, surface, cam_x):
        pygame.draw.rect(surface, self.color,
                         (self.rect.x - cam_x, self.rect.y, self.rect.w, self.rect.h))

class MovingPlatform(Platform):
    def __init__(self, x, y, w, h, range_x):
        super().__init__(x, y, w, h)
        self.start_x = x
        self.range_x = range_x
        self.direction = 1
        self.speed = 2
        self.color = (200, 150, 0)

    def update(self):
        self.rect.x += self.speed * self.direction
        if abs(self.rect.x - self.start_x) > self.range_x:
            self.direction *= -1

class CollapsePlatform(Platform):
    def __init__(self, x, y, w, h):
        super().__init__(x, y, w, h)
        self.timer = 0
        self.active = True
        self.color = (255, 100, 100)

    def update(self):
        if self.timer > 0:
            self.timer -= 1
            if self.timer <= 0:
                self.active = False

    def trigger(self):
        if self.timer == 0:
            self.timer = FPS

class SpikeTrap(Platform):
    def __init__(self, x, y, w, h):
        super().__init__(x, y, w, h)
        self.timer = 0
        self.state = 0

    def update(self):
        self.timer += 1
        if self.timer > FPS:
            self.timer = 0
            self.state = 1 - self.state

    def draw(self, surface, cam_x):
        if self.state:
            pygame.draw.rect(surface, (255, 0, 0),
                             (self.rect.x - cam_x, self.rect.y, self.rect.w, self.rect.h))

# =========================
# ENEMY
# =========================
class Enemy(pygame.sprite.Sprite):
    def __init__(self, x, y):
        super().__init__()
        self.image = pygame.Surface((40, 40))
        self.image.fill((200, 50, 50))
        self.rect = self.image.get_rect(topleft=(x, y))
        self.direction = 1
        self.speed = 2

    def update(self):
        self.rect.x += self.speed * self.direction
        if self.rect.x < 300 or self.rect.x > 600:
            self.direction *= -1

    def draw(self, surface, cam_x):
        surface.blit(self.image, (self.rect.x - cam_x, self.rect.y))

# =========================
# KEY & DOOR
# =========================
class Key:
    def __init__(self, x, y):
        self.rect = pygame.Rect(x, y, 20, 20)

    def draw(self, surface, cam_x):
        pygame.draw.rect(surface, (255, 255, 0),
                         (self.rect.x - cam_x, self.rect.y, 20, 20))

class Door:
    def __init__(self, x, y):
        self.rect = pygame.Rect(x, y, 60, 100)
        self.locked = True

    def draw(self, surface, cam_x):
        color = (0, 200, 0) if not self.locked else (50, 50, 50)
        pygame.draw.rect(surface, color,
                         (self.rect.x - cam_x, self.rect.y, 60, 100))

# =========================
# PAUSE MENU
# =========================
class PauseMenu:
    def __init__(self):
        self.options = ["Resume", "Reset", "Achievements", "Quit"]
        self.selected = 0

    def draw(self, surface):
        overlay = pygame.Surface((WIDTH, HEIGHT))
        overlay.set_alpha(180)
        overlay.fill((0, 0, 0))
        surface.blit(overlay, (0, 0))

        for i, text in enumerate(self.options):
            color = (255, 255, 255) if i == self.selected else (150, 150, 150)
            render = font.render(text, True, color)
            surface.blit(render, (WIDTH//2 - 100, 200 + i*50))

    def move(self, direction):
        self.selected = (self.selected + direction) % len(self.options)

# =========================
# GAME
# =========================
class Game:
    def __init__(self):
        self.reset()

    def reset(self):
        self.player = Player(100, 300)
        self.platforms = [
            Platform(0, 400, 1200, 40),
            MovingPlatform(500, 300, 100, 20, 150),
            CollapsePlatform(800, 350, 100, 20),
            SpikeTrap(1000, 380, 100, 20)
        ]
        self.enemy = Enemy(500, 360)
        self.keys = [Key(200, 350), Key(700, 250), Key(1100, 350)]
        self.door = Door(1150, 300)
        self.collected = 0
        self.camera_x = 0
        self.paused = False
        self.pause_menu = PauseMenu()
        self.achievements = []

    def update(self):
        if self.paused:
            return

        for p in self.platforms:
            p.update()

        self.enemy.update()
        self.player.update([p for p in self.platforms if not isinstance(p, CollapsePlatform) or p.active])

        # Key collection
        for key in self.keys[:]:
            if self.player.rect.colliderect(key.rect):
                self.keys.remove(key)
                self.collected += 1

        if self.collected == 3:
            self.door.locked = False

        if self.player.rect.colliderect(self.enemy.rect):
            self.reset()

        # Camera
        self.camera_x = self.player.rect.centerx - WIDTH//2

    def draw(self):
        screen.fill((30, 30, 50))

        for p in self.platforms:
            if not isinstance(p, CollapsePlatform) or p.active:
                p.draw(screen, self.camera_x)

        for key in self.keys:
            key.draw(screen, self.camera_x)

        self.door.draw(screen, self.camera_x)
        self.enemy.draw(screen, self.camera_x)
        screen.blit(self.player.image,
                    (self.player.rect.x - self.camera_x, self.player.rect.y))

        text = font.render(f"Keys: {self.collected}/3", True, (255, 255, 255))
        screen.blit(text, (20, 20))

        if self.paused:
            self.pause_menu.draw(screen)

    def handle_events(self, event):
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_ESCAPE:
                self.paused = not self.paused

            if self.paused:
                if event.key == pygame.K_UP:
                    self.pause_menu.move(-1)
                if event.key == pygame.K_DOWN:
                    self.pause_menu.move(1)
                if event.key == pygame.K_RETURN:
                    option = self.pause_menu.options[self.pause_menu.selected]
                    if option == "Resume":
                        self.paused = False
                    elif option == "Reset":
                        self.reset()
                    elif option == "Quit":
                        pygame.quit()
                        sys.exit()

# =========================
# MAIN
# =========================
if __name__ == "__main__":
    game = Game()

    while True:
        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            game.handle_events(event)

        game.update()
        game.draw()
        pygame.display.flip()
