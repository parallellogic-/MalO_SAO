#tictactoe py code that is object oriented and have main call in __name__=="__main__": block, using pygame, with AI opponent.  add option for game mode where if there are more than 6 pieces on the board, and no player has one, the oldest piece is removed.  indicate which piece will be removed next turn before it is removed so the player can make decisions on this information.  add a new object for a pause menu that allows selections like viewing achievements and exiting the game, resetting the game, etc
import pygame
import sys
import math


# ==============================
# Pause Menu Object
# ==============================
class PauseMenu:
    def __init__(self, game):
        self.game = game
        self.options = [
            "Resume",
            "Reset Game",
            "Toggle Aging Mode",
            "View Achievements",
            "Exit"
        ]
        self.selected = 0
        self.font = pygame.font.SysFont(None, 40)
        self.small_font = pygame.font.SysFont(None, 28)

    def draw(self):
        overlay = pygame.Surface((self.game.width, self.game.height))
        overlay.set_alpha(200)
        overlay.fill((0, 0, 0))
        self.game.screen.blit(overlay, (0, 0))

        for i, option in enumerate(self.options):
            color = (255, 255, 255)
            if i == self.selected:
                color = (255, 200, 0)

            text = self.font.render(option, True, color)
            self.game.screen.blit(
                text,
                (self.game.width // 2 - text.get_width() // 2,
                 200 + i * 60)
            )

    def handle_input(self, event):
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_UP:
                self.selected = (self.selected - 1) % len(self.options)
            elif event.key == pygame.K_DOWN:
                self.selected = (self.selected + 1) % len(self.options)
            elif event.key == pygame.K_RETURN:
                self.activate_option()

    def activate_option(self):
        option = self.options[self.selected]

        if option == "Resume":
            self.game.paused = False

        elif option == "Reset Game":
            self.game.reset()
            self.game.paused = False

        elif option == "Toggle Aging Mode":
            self.game.aging_mode = not self.game.aging_mode

        elif option == "View Achievements":
            self.show_achievements()

        elif option == "Exit":
            pygame.quit()
            sys.exit()

    def show_achievements(self):
        showing = True
        while showing:
            self.game.screen.fill((30, 30, 30))

            lines = [
                "Achievements",
                f"Games Played: {self.game.games_played}",
                f"Player Wins: {self.game.player_wins}",
                f"AI Wins: {self.game.ai_wins}",
                "Press ESC to return"
            ]

            for i, line in enumerate(lines):
                text = self.font.render(line, True, (255, 255, 255))
                self.game.screen.blit(
                    text,
                    (self.game.width // 2 - text.get_width() // 2,
                     150 + i * 60)
                )

            pygame.display.update()

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    showing = False


# ==============================
# Main Game Object
# ==============================
class TicTacToe:
    def __init__(self, width=600, height=650):
        pygame.init()
        self.width = width
        self.height = height
        self.board_size = 600
        self.screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption("Tic Tac Toe - Advanced")

        self.cell_size = self.board_size // 3
        self.line_width = 6
        self.symbol_padding = 40

        self.bg_color = (28, 170, 156)
        self.line_color = (23, 145, 135)
        self.highlight_color = (200, 50, 50)

        self.font = pygame.font.SysFont(None, 28)

        self.games_played = 0
        self.player_wins = 0
        self.ai_wins = 0

        self.paused = False
        self.pause_menu = PauseMenu(self)

        self.reset()

    def reset(self):
        self.board = [["" for _ in range(3)] for _ in range(3)]
        self.move_history = []
        self.current_player = "X"
        self.game_over = False

    # ---------------- Game Logic ----------------
    def place_piece(self, r, c, player):
        self.board[r][c] = player
        self.move_history.append((r, c))

        if self.aging_mode and len(self.move_history) > 6:
            if not self.check_winner(self.board):
                old_r, old_c = self.move_history.pop(0)
                self.board[old_r][old_c] = ""

    def check_winner(self, board):
        lines = []

        lines.extend(board)
        lines.extend([[board[r][c] for r in range(3)] for c in range(3)])
        lines.append([board[i][i] for i in range(3)])
        lines.append([board[i][2 - i] for i in range(3)])

        for line in lines:
            if line[0] == line[1] == line[2] != "":
                return line[0]
        return None

    def is_draw(self):
        return all(cell != "" for row in self.board for cell in row)

    # ---------------- AI ----------------
    def minimax(self, board, maximizing):
        winner = self.check_winner(board)
        if winner == "O":
            return 1
        elif winner == "X":
            return -1
        elif self.is_draw():
            return 0

        if maximizing:
            best = -math.inf
            for r in range(3):
                for c in range(3):
                    if board[r][c] == "":
                        board[r][c] = "O"
                        score = self.minimax(board, False)
                        board[r][c] = ""
                        best = max(best, score)
            return best
        else:
            best = math.inf
            for r in range(3):
                for c in range(3):
                    if board[r][c] == "":
                        board[r][c] = "X"
                        score = self.minimax(board, True)
                        board[r][c] = ""
                        best = min(best, score)
            return best

    def ai_move(self):
        best_score = -math.inf
        move = None

        for r in range(3):
            for c in range(3):
                if self.board[r][c] == "":
                    self.board[r][c] = "O"
                    score = self.minimax(self.board, False)
                    self.board[r][c] = ""
                    if score > best_score:
                        best_score = score
                        move = (r, c)

        if move:
            self.place_piece(move[0], move[1], "O")

    # ---------------- Drawing ----------------
    def draw(self):
        self.screen.fill(self.bg_color)

        for i in range(1, 3):
            pygame.draw.line(self.screen, self.line_color,
                             (0, i * self.cell_size),
                             (self.board_size, i * self.cell_size), 4)
            pygame.draw.line(self.screen, self.line_color,
                             (i * self.cell_size, 0),
                             (i * self.cell_size, self.board_size), 4)

        for r in range(3):
            for c in range(3):
                if self.board[r][c] != "":
                    text = pygame.font.SysFont(None, 80).render(
                        self.board[r][c], True, (0, 0, 0))
                    self.screen.blit(
                        text,
                        (c * self.cell_size + 80,
                         r * self.cell_size + 60)
                    )

        if self.aging_mode and len(self.move_history) > 6:
            r, c = self.move_history[0]
            rect = pygame.Rect(
                c * self.cell_size,
                r * self.cell_size,
                self.cell_size,
                self.cell_size
            )
            pygame.draw.rect(self.screen, self.highlight_color, rect, 4)

        mode = "Aging Mode ON" if self.aging_mode else "Normal Mode"
        text = self.font.render(mode + " | ESC=Pause", True, (0, 0, 0))
        self.screen.blit(text, (20, 610))

    # ---------------- Main Loop ----------------
    def run(self):
        clock = pygame.time.Clock()
        self.aging_mode = False

        while True:
            if not self.paused:
                self.draw()

                winner = self.check_winner(self.board)
                if winner and not self.game_over:
                    self.game_over = True
                    self.games_played += 1
                    if winner == "X":
                        self.player_wins += 1
                    else:
                        self.ai_wins += 1

                if not self.game_over and self.current_player == "O":
                    self.ai_move()
                    self.current_player = "X"

            else:
                self.pause_menu.draw()

            pygame.display.update()

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()

                if self.paused:
                    self.pause_menu.handle_input(event)
                else:
                    if event.type == pygame.KEYDOWN:
                        if event.key == pygame.K_ESCAPE:
                            self.paused = True
                        if event.key == pygame.K_m:
                            self.aging_mode = not self.aging_mode
                        if event.key == pygame.K_r:
                            self.reset()

                    if event.type == pygame.MOUSEBUTTONDOWN and not self.game_over:
                        x, y = event.pos
                        if y < self.board_size:
                            r = y // self.cell_size
                            c = x // self.cell_size
                            if self.board[r][c] == "":
                                self.place_piece(r, c, "X")
                                self.current_player = "O"

            clock.tick(60)


if __name__ == "__main__":
    game = TicTacToe()
    game.run()
