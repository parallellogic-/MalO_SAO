import pygame
import sys

# Initialize Pygame
pygame.init()

# Constants
WIDTH = 300
HEIGHT = 300
LINE_WIDTH = 10
BOARD_ROWS = 3
BOARD_COLS = 3
SQUARE_SIZE = WIDTH // BOARD_COLS
CIRCLE_RADIUS = SQUARE_SIZE / 2 - 5
CROSS_WIDTH = 20
SPACE = 50

# Colors
WHITE = (255, 255, 255)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)

# Screen setup
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption('Tic-Tac-Toe')

# Board setup
board = [['' for _ in range(BOARD_COLS)] for _ in range(BOARD_ROWS)]
player = 1  # Player 1 starts

def draw_lines():
    """Draw the board lines."""
    for i in range(1, BOARD_COLS):
        pygame.draw.line(screen, BLUE, (i * SQUARE_SIZE, 0), (i * 
SQUARE_SIZE, HEIGHT))
        pygame.draw.line(screen, BLUE, (0, i * SQUARE_SIZE), (WIDTH, i * 
SQUARE_SIZE))

def draw_figures():
    """Draw the X's and O's on the board."""
    for row in range(BOARD_ROWS):
        for col in range(BOARD_COLS):
            if board[row][col] == 'X':
                pygame.draw.line(screen, RED, (col * SQUARE_SIZE + SPACE, 
row * SQUARE_SIZE + SPACE), 
                                 (col * SQUARE_SIZE + SQUARE_SIZE - SPACE, 
row * SQUARE_SIZE + SQUARE_SIZE - SPACE), CROSS_WIDTH)
                pygame.draw.line(screen, RED, (col * SQUARE_SIZE + SPACE, 
row * SQUARE_SIZE + SQUARE_SIZE - SPACE), 
                                 (col * SQUARE_SIZE + SQUARE_SIZE - SPACE, 
row * SQUARE_SIZE + SPACE), CROSS_WIDTH)
            elif board[row][col] == 'O':
                pygame.draw.circle(screen, GREEN, (col * SQUARE_SIZE + 
SQUARE_SIZE // 2, row * SQUARE_SIZE + SQUARE_SIZE // 2), CIRCLE_RADIUS, 
LINE_WIDTH)

def mark_square(row, col, player):
    """Mark the square at (row, col) for the given player."""
    board[row][col] = 'X' if player == 1 else 'O'

def available_square():
    """Check for available squares on the board."""
    return [[r, c] for r in range(BOARD_ROWS) for c in range(BOARD_COLS) 
if board[r][c] == '']

def check_win(player):
    """Check if the given player has won."""
    # Check rows
    for row in range(BOARD_ROWS):
        if all([board[row][col] == player for col in range(BOARD_COLS)]):
            return True
    # Check columns
    for col in range(BOARD_COLS):
        if all([board[row][col] == player for row in range(BOARD_ROWS)]):
            return True
    # Check diagonals
    if all([board[i][i] == player for i in range(BOARD_COLS)]):
        return True
    if all([board[i][BOARD_COLS - 1 - i] == player for i in 
range(BOARD_COLS)]):
        return True
    return False

def check_draw():
    """Check if the game is a draw."""
    return not any('' in row for row in board)

while True:
    screen.fill(WHITE)
    draw_lines()
    
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()
        
        if event.type == pygame.MOUSEBUTTONDOWN:
            mouseX, mouseY = event.pos
            row, col = mouseY // SQUARE_SIZE, mouseX // SQUARE_SIZE
            
            if board[row][col] == '' and available_square():
                mark_square(row, col, player)
                
                if check_win(player):
                    print(f"Player {player} wins!")
                    pygame.quit()
                    sys.exit()
                
                if check_draw():
                    print("It's a draw!")
                    pygame.quit()
                    sys.exit()
                
                player = 2 if player == 1 else 1
    
    draw_figures()
    pygame.display.update()
