import sys
import twitchio
from twitchio.ext import commands
import random
import re
import threading
import yt_dlp
import os
import http.server
import socketserver
import webbrowser
import asyncio
import json
import configparser
import websockets
import pyautogui
import requests

# Initialize safety settings
pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.1

# WebSocket server port
WS_PORT = 8080

class Bot(commands.Bot):
    def __init__(self, token, channel):
        super().__init__(token=token, prefix='!', initial_channels=[channel])
        self.websocket = None

    async def event_ready(self):
        print(f'Logged in as | {self.nick}')
        await self.start_websocket_server()

    async def start_websocket_server(self):
        async def websocket_handler(websocket, path):
            self.websocket = websocket
            try:
                async for message in websocket:
                    if message == "NEXT_SONG":
                        pass  # No queue functionality
            except websockets.exceptions.ConnectionClosed as e:
                print(f"WebSocket closed: {e.code}. Reconnecting...")
            except Exception as e:
                print(f"WebSocket error: {str(e)}")
            finally:
                self.websocket = None

        while True:
            try:
                server = await websockets.serve(websocket_handler, "localhost", WS_PORT)
                print(f"WebSocket server started on ws://localhost:{WS_PORT}")
                await server.wait_closed()
            except Exception as e:
                print(f"WebSocket error: {str(e)}. Retrying in 2 seconds...")
                await asyncio.sleep(2)

    async def event_message(self, message):
        if message.echo:
            return
        print(f"{message.author.name}: {message.content}")
        await self.handle_commands(message)

    @commands.command(name='pc')
    async def pc_control(self, ctx: commands.Context, *, command_string: str):
        """Allow all users to use !pc commands."""
        try:
            # Split commands while ignoring empty entries
            raw_commands = [c.strip() for c in command_string.split(',') if c.strip()]
            
            for cmd in raw_commands:
                # Split into action and arguments
                parts = cmd.split(maxsplit=1)
                action = parts[0].lower()
                args = parts[1] if len(parts) > 1 else ''

                # Command handling
                if action == 'write':
                    pyautogui.write(args)
                elif action == 'enter':
                    pyautogui.press('enter')
                elif action == 'click':
                    pyautogui.click()
                elif action == 'move':
                    x, y = map(int, args.split())
                    pyautogui.moveTo(x, y)
                elif action == 'press':
                    pyautogui.press(args.lower())
                else:
                    await ctx.send(f"❌ Unknown command: {action}")
                    continue
                    
                await asyncio.sleep(0.5)  # Delay between commands

            await ctx.send(f"✅ Executed {len(raw_commands)} command(s)")
            
        except Exception as e:
            error_msg = f"❌ Error: {str(e)}"
            print(f"PC Control Error: {error_msg}")
            await ctx.send(error_msg[:400])  # Truncate long errors

def load_config():
    config = configparser.ConfigParser()
    config_file = 'config.ini'
    
    if not os.path.exists(config_file):
        config['Twitch'] = {
            'oauth_token': 'oauth:your_token_here',
            'channel': 'your_channel_here'
        }
        with open(config_file, 'w') as configfile:
            config.write(configfile)
        print("Created config.ini - edit it!")
        sys.exit(1)
    
    config.read(config_file)
    return config['Twitch']['oauth_token'], config['Twitch']['channel']

def main():
    token, channel = load_config()

    bot = Bot(token, channel)
    try:
        bot.run()
    finally:
        print("Bot stopped.")

if __name__ == "__main__":
    main()