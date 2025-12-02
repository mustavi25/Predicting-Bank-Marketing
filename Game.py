# Lab 02: Midpoint Line Drawing Algorithm Implementation

from OpenGL.GL import *
from OpenGL.GLUT import *
from OpenGL.GLU import *
import random


SCREEN_W, SCREEN_H = 500, 500


basket_x = 0
basket_y = -230
basket_width = 70
basket_height = 20
basket_flash_color = 0.0


gem_width = 40
gem_height = 40
gem_x = random.randint(-250, 210)
gem_top = 250
fall_velocity = 0


is_paused = False
is_game_over = False
player_score = 0


color_r = random.randint(0, 1)
color_g = random.randint(0, 1)
color_b = random.randint(0, 1)


# ===== Midpoint Line Drawing Algorithm =====
def determine_zone(x1, y1, x2, y2):
    """Determine which zone the line belongs to (0-7)"""
    delta_x = x2 - x1
    delta_y = y2 - y1
    
    if abs(delta_y) > abs(delta_x):
        if delta_y > 0 and delta_x > 0:
            return 1
        elif delta_y > 0 and delta_x < 0:
            return 2
        elif delta_y < 0 and delta_x > 0:
            return 6
        elif delta_y < 0 and delta_x < 0:
            return 5
    else:
        if delta_y > 0 and delta_x > 0:
            return 0
        elif delta_y > 0 and delta_x < 0:
            return 3
        elif delta_y < 0 and delta_x > 0:
            return 7
        elif delta_y < 0 and delta_x < 0:
            return 4
    return 0


def transform_to_zone0(zone, x, y):
    """Convert coordinates from any zone to Zone 0"""
    if zone == 0:
        return x, y
    elif zone == 1:
        return y, x
    elif zone == 2:
        return -y, x
    elif zone == 3:
        return -x, y
    elif zone == 4:
        return -x, -y
    elif zone == 5:
        return -y, -x
    elif zone == 6:
        return -y, x
    elif zone == 7:
        return x, -y
    return x, y


def restore_from_zone0(zone, x, y):
    """Convert coordinates from Zone 0 back to original zone"""
    if zone == 0:
        return x, y
    elif zone == 1:
        return y, x
    elif zone == 2:
        return -y, x
    elif zone == 3:
        return -x, y
    elif zone == 4:
        return -x, -y
    elif zone == 5:
        return -y, -x
    elif zone == 6:
        return y, -x
    elif zone == 7:
        return x, -y
    return x, y


def draw_midpoint_line(x1, y1, x2, y2):
    """Draw line using midpoint algorithm"""
    zone = determine_zone(x1, y1, x2, y2)
    X1, Y1 = transform_to_zone0(zone, x1, y1)
    X2, Y2 = transform_to_zone0(zone, x2, y2)
    
    # Handle vertical lines
    if X1 == X2:
        glPointSize(3)
        glBegin(GL_POINTS)
        if Y1 > Y2:
            Y1, Y2 = Y2, Y1
        for y in range(Y1, Y2 + 1):
            orig_x, orig_y = restore_from_zone0(zone, X1, y)
            glVertex2f(orig_x, orig_y)
        glEnd()
        return
    
    # Ensure X1 < X2
    if X2 < X1:
        X1, X2 = X2, X1
        Y1, Y2 = Y2, Y1
    
    dx = X2 - X1
    dy = Y2 - Y1
    decision = 2 * dy - dx
    increment_e = 2 * dy
    increment_ne = 2 * (dy - dx)
    
    glPointSize(3)
    glBegin(GL_POINTS)
    while X1 < X2:
        original_x, original_y = restore_from_zone0(zone, X1, Y1)
        glVertex2f(original_x, original_y)
        
        if decision > 0:
            X1 += 1
            Y1 += 1
            decision += increment_ne
        else:
            X1 += 1
            decision += increment_e
    glEnd()



def render_basket():
    """Draw the catcher basket"""
    global basket_x, basket_y, basket_width, basket_height, basket_flash_color
    
    glColor3f(1.0, abs(1.0 - basket_flash_color), abs(1.0 - basket_flash_color))
    
    left_edge = basket_x - (basket_width // 2)
    right_edge = basket_x + (basket_width // 2)
    top_edge = basket_y + basket_height
    bottom_edge = basket_y
    
    # Top horizontal line
    draw_midpoint_line(left_edge, top_edge, right_edge, top_edge)
    # Left slant
    draw_midpoint_line(left_edge, top_edge, left_edge + 10, bottom_edge)
    # Right slant
    draw_midpoint_line(right_edge, top_edge, right_edge - 10, bottom_edge)
    # Bottom horizontal line
    draw_midpoint_line(right_edge - 10, bottom_edge, left_edge + 10, bottom_edge)


def render_gem():
    """Draw the falling diamond"""
    global gem_x, gem_top, gem_width, gem_height, color_r, color_g, color_b
    
    glColor3f(color_r, color_g, color_b)
    
    gem_bottom = gem_top - gem_height
    gem_right = gem_x + gem_width
    half_height = gem_height // 2
    

    draw_midpoint_line(gem_x + 20, gem_top, gem_x, gem_top - half_height)
    draw_midpoint_line(gem_x + 20, gem_top, gem_right, gem_top - half_height)
    draw_midpoint_line(gem_x, gem_top - half_height, gem_x + 20, gem_bottom)
    draw_midpoint_line(gem_right, gem_top - half_height, gem_x + 20, gem_bottom)


def render_control_buttons():
    """Draw the three control buttons at top"""
    global is_paused
    

    glColor3f(1.0, 0.0, 0.0)
    draw_midpoint_line(-230, 220, -210, 220)
    draw_midpoint_line(-230, 220, -220, 230)
    draw_midpoint_line(-230, 220, -220, 210)
    
    glColor3f(1.0, 1.0, 0.0)
    if is_paused == False:
        
        draw_midpoint_line(-5, 220, -5, 240)
        draw_midpoint_line(5, 220, 5, 240)
    else:
        
        draw_midpoint_line(-5, 220, -5, 240)
        draw_midpoint_line(-5, 220, 0, 230)
        draw_midpoint_line(-5, 240, 0, 230)
    
    
    glColor3f(1.0, 0.0, 0.0)
    draw_midpoint_line(230, 220, 250, 240)
    draw_midpoint_line(250, 220, 230, 240)



def detect_collision():
    """Check if diamond hits the catcher"""
    global basket_x, basket_y, basket_width, basket_height
    global gem_x, gem_top, gem_width, gem_height
    
    gem_bottom = gem_top - gem_height
    gem_right = gem_x + gem_width
    left_edge = basket_x - (basket_width // 2)
    right_edge = basket_x + (basket_width // 2)
    top_edge = basket_y + basket_height
    bottom_edge = basket_y
    
    if gem_right > left_edge and gem_x < right_edge and gem_bottom < top_edge and gem_top > bottom_edge:
        return True
    return False



def restart_game():
    """Reset all game variables"""
    global basket_x, basket_y, basket_width, basket_height, basket_flash_color
    global gem_width, gem_height, gem_x, gem_top
    global is_paused, is_game_over, player_score, fall_velocity
    global color_r, color_g, color_b
    
    basket_flash_color = 0.0
    basket_x = 0
    basket_y = -230
    basket_width = 70
    basket_height = 20
    gem_width = 40
    gem_height = 40
    gem_x = random.randint(-250, 210)
    gem_top = 250
    is_paused = False
    is_game_over = False
    fall_velocity = 0
    player_score = 0
    color_r = random.randint(0, 1)
    color_g = random.randint(0, 1)
    color_b = random.randint(0, 1)
    
    print("Game restarted!")



def handle_special_keys(key, x, y):
    """Handle arrow key inputs"""
    global basket_x, is_game_over, is_paused
    
    if is_game_over == False:
        if is_paused == False and is_game_over == False:
            if key == GLUT_KEY_LEFT:
                basket_x = max(basket_x - 20, -225)
            elif key == GLUT_KEY_RIGHT:
                basket_x = min(basket_x + 20, 225)
    
    glutPostRedisplay()


def handle_mouse_click(button, state, x, y):
    """Handle mouse button clicks"""
    global SCREEN_W, SCREEN_H, is_paused
    
    if button == GLUT_LEFT_BUTTON and state == GLUT_DOWN:
        y = SCREEN_H - y
        x_coord = x - (SCREEN_W // 2)
        y_coord = y - (SCREEN_H // 2)
        
    
        if -5 < x_coord < 5 and 220 < y_coord < 240:
            is_paused = not is_paused
            if is_paused:
                print("Game paused")
            else:
                print("Game resumed")
        
        # Restart button
        elif -230 <= x_coord <= -210 and 210 <= y_coord <= 220:
            restart_game()
        
        # Exit button
        elif 230 <= x_coord <= 250 and 220 <= y_coord <= 240:
            print(f"Goodbye! Final Score: {player_score}")
            glutLeaveMainLoop()
    
    glutPostRedisplay()



def render_display():
    """Main display function"""
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glClearColor(0, 0, 0, 0)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    gluLookAt(0, 0, 200, 0, 0, 0, 0, 1, 0)
    
    render_basket()
    render_gem()
    render_control_buttons()
    
    glutSwapBuffers()


def update_animation():
    """Animation loop"""
    global gem_top, is_paused, player_score, is_game_over, fall_velocity
    global gem_height, basket_flash_color, gem_x, color_r, color_g, color_b
    
    if is_paused == False and is_game_over == False:
        gem_top -= 0.1 + fall_velocity
        gem_bottom = gem_top - gem_height
        
        if gem_bottom < -250:
            print(f"Game Over! Final Score: {player_score}")
            is_game_over = True
            player_score = 0
            basket_flash_color = 1
            gem_top -= 50
    
    if detect_collision():
        player_score += 1
        print(f"Score: {player_score}")
        gem_x = random.randint(-250, 210)
        gem_top = 250
        fall_velocity += 0.01
        color_r = random.randint(0, 1)
        color_g = random.randint(0, 1)
        color_b = random.randint(0, 1)
    
    glutPostRedisplay()


def setup_projection():
    """Initialize OpenGL projection"""
    glClearColor(0, 0, 0, 0)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(104, 1, 1, 1000.0)



def main():
    glutInit()
    glutInitWindowSize(SCREEN_W, SCREEN_H)
    glutInitWindowPosition(100, 100)
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGB)
    glutCreateWindow(b"Catch the Diamonds Game")
    
    setup_projection()
    
    glutDisplayFunc(render_display)
    glutIdleFunc(update_animation)
    glutSpecialFunc(handle_special_keys)
    glutMouseFunc(handle_mouse_click)
    
    print("=== Catch the Diamonds ===")
    print("Controls:")
    print("  LEFT/RIGHT arrows: Move catcher")
    print("  Click restart button (left arrow): Restart")
    print("  Click pause button (middle): Pause/Resume")
    print("  Click exit button (X): Quit")
    print(f"Score: {player_score}")
    
    glutMainLoop()


if __name__ == "__main__":
    main()