import os, sys              #Modificacao
from fcntl import ioctl
from integracao import * 

from src.runner import GameRun

if __name__=='__main__':
    gr = GameRun()
    
    #Modificacao
    if gr.io.get_SW(0): 
        gr.main()
        
#Ou fzr self.io = IO() if io.get_sw(0): ...
