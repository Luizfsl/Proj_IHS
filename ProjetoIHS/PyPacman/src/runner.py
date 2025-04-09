import os
from fcntl import ioctl
from integracao import *

import sys

import pygame
import json

import time

from src.configs import *
from src.game.event_management import EventHandler
from src.game.state_management import GameState
from src.gui.screen_management import ScreenManager
from src.sounds import SoundManager
from src.log_handle import get_logger
logger = get_logger(__name__)

class GameRun:
    def __init__(self):
        logger.info("About to initialize pygame")
        pygame.init()
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("Py-Pacman")
        logger.info("pygame initialized")
        self.game_state = GameState()
        logger.info("game state object created")
        self.events = EventHandler(self.screen, self.game_state)
        logger.info("event handler object created")
        self.all_sprites = pygame.sprite.Group()
        self.gui = ScreenManager(self.screen, self.game_state, self.all_sprites)
        logger.info("screen manager object created")
         
        # IO setup
        self.io = IO()
        
    # Modificacao    
    def update_display(self):
        # Atualiza o display de 7 segmentos com a pontuação e o highscore
        score_str = str(self.game_state.points).zfill(4)  # Certifica-se de que a pontuação tenha 4 dígitos
        highscore_str = str(self.game_state.highscore).zfill(4)  # Certifica-se de que o highscore tenha 4 dígitos

        self.io.put_DP(0, score_str)  # Atualiza o display direito com a pontuação
        self.io.put_DP(1, highscore_str)  # Atualiza o display esquerdo com o highscore
        
    # Modificacao    
    def finish_display(self):
        # Atualiza o display de 7 segmentos com a pontuação e o highscore
        score_str = "0000"
        highscore_str = "0000"
        
        time.sleep(0.5)  # Pequeno atraso para garantir atualização
        self.io.put_DP(0, score_str)  # Atualiza o display direito com a pontuação
        self.io.put_DP(1, highscore_str)  # Atualiza o display esquerdo com o highscore
        
    def update_led_score(self):
        # Calcular o número de LEDs a serem acesos com base na pontuação
        num_leds_to_light = self.game_state.points // 100       # substituir 100 por outro numero dependendo do quanto cresce os pontos (ligar mais leds pro video ficar bonito msm)
        array = []
        # Atualizar o estado dos LEDs
        for i in range(num_leds_to_light):
            array.append(i)
            self.io.put_ar_LD(array)  # Substitua por sua função específica para acender LEDs
    
    # Funcao de inicializacao dos leds
    def iniciar_leds(self):
        array = []
        self.io.put_ar_LD(array)

    def initialize_highscore(self):
        with open("levels/stats.json") as fp:
            stats = json.load(fp)
            self.game_state.highscore = stats['highscore']
            self.game_state.mins_played = stats['mins_played']
    
    def create_ghost_mode_event(self):
        CUSTOM_EVENT = pygame.USEREVENT + 1
        pygame.time.set_timer(CUSTOM_EVENT, 
                              self.game_state.mode_change_events * 1000)
        self.game_state.custom_event = CUSTOM_EVENT

    def initialize_sounds(self):
        sound_manager = SoundManager()
        sound_manager.load_sound("dot", "assets/sounds/pacman_chomp.wav", channel=0)
        sound_manager.load_sound("death","assets/sounds/pacman_death.wav", 0.7, 500, 1)
        sound_manager.load_sound("eat_ghost","assets/sounds/pacman_eatghost.wav", 0.6, 100, 2)
        sound_manager.set_background_music("assets/sounds/backgroud.mp3")
        sound_manager.play_background_music()

    def check_highscores(self):
        if self.game_state.points > self.game_state.highscore:
            self.game_state.highscore = self.game_state.points

    def update_highscore(self):
        with open("levels/stats.json", 'w') as fp:
            json.dump({"highscore":self.game_state.highscore,
                       "mins_played": self.game_state.mins_played}, fp, indent=4)
            
    def main(self):
        clock = pygame.time.Clock()
        dt = None
        self.create_ghost_mode_event()
        self.initialize_sounds()
        self.initialize_highscore()
        self.iniciar_leds()             #Modificacao
        self.update_display()           #Modificacao
        
        last_score = -1             #Modificacao
        last_highscore = -1         #Modificacao
                
        while self.game_state.running:
    
            # Modificação: Verifica o switch 2 antes de continuar a execução
            if self.io.get_SW(2):  
                self.game_state.running = False  # Sai do loop principal
                self.iniciar_leds()  # Modificação: Reseta os LEDs ao sair do jogo
                self.finish_display()  # Modificação: Reseta o display de 7 segmentos ao sair do jogo
                break  # Garante que o loop seja interrompido imediatamente
            
            if self.game_state.points != last_score:
                self.update_display()
                self.update_led_score()
                last_score = self.game_state.points
                last_highscore = self.game_state.highscore

            self.game_state.current_time = pygame.time.get_ticks()
            for event in pygame.event.get():
                self.events.handle_events(event)
            self.screen.fill(Colors.BLACK)
            self.gui.draw_screens()
            self.all_sprites.draw(self.screen)
            self.all_sprites.update(dt)
            self.check_highscores()
            pygame.display.flip()
            dt = clock.tick(self.game_state.fps)
            dt /= 100

        self.update_highscore()
        self.iniciar_leds()  # Modificação: Reseta os LEDs ao sair do jogo
        self.finish_display()

        pygame.quit()
        sys.exit()
