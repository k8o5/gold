import asyncio
import os
from dotenv import load_dotenv
from browser_use import Agent
from langchain_google_genai import ChatGoogleGenerativeAI
import traceback

# Load environment variables from .env file (if it exists)
load_dotenv()

# --- Configuration & Constants ---
GOOGLE_API_KEY_HELP_URL = "https://aistudio.google.com/app/apikey"
# --- USER SPECIFIED GOOGLE MODEL ---
# KORRIGIERT und auf Ihr ursprüngliches Modell zurückgesetzt:
TARGET_GOOGLE_MODEL = "gemini-2.5-flash-preview-05-20"
# ---------------------------------

async def run_browser_agent_task(user_task: str, llm_instance: ChatGoogleGenerativeAI):
    """
    Runs the browser-use agent with the specified task using the provided LLM instance.
    """
    # Access the model name via llm_instance.model
    current_model_name = llm_instance.model
    print(f"\n🤖 Starting new task: \"{user_task}\" using Google Gemini model: {current_model_name}")
    print("💡 This might take a few moments to start up and then a few minutes to run...")

    if not llm_instance:
        print("🔴 Error: LLM instance was not provided to run_browser_agent_task.")
        return

    try:
        agent = Agent(
            task=user_task,
            llm=llm_instance,
            # storage_path="browser_agent_storage" # Optional: for session persistence
        )

        print("\n🚀 Agent is starting its work...")
        await agent.run()

        print("\n✅ Agent has completed the task (or tried its best!).")
        print("   Check the console output above for the agent's actions and findings.")

    except Exception as e:
        print(f"🔴 An error occurred during agent execution for task '{user_task}': {e}")
        traceback.print_exc()
    finally:
        print("-" * 50) # Separator for clarity between tasks

async def main():
    print("🌟 Welcome to the Google Gemini Browser Agent! 🌟")
    # Use TARGET_GOOGLE_MODEL for the initial message
    print(f"This script uses browser-use with your specified Google Gemini model (configured as: {TARGET_GOOGLE_MODEL}).")
    print("It's recommended to store your GOOGLE_API_KEY in a .env file for convenience.")

    # --- Get Google API Key ---
    google_api_key = os.getenv("GOOGLE_API_KEY")
    if not google_api_key:
        print("\n🟡 GOOGLE_API_KEY not found in .env file or environment variables.")
        print(f"   You can get an API key from Google AI Studio: {GOOGLE_API_KEY_HELP_URL}")
        key_input = input("   Please paste your Google API key here (or press Enter to skip): ").strip()
        if not key_input:
            print("🔴 No Google API key provided. Exiting.")
            return
        google_api_key = key_input
        print("   Using provided Google API key for this session.")
    else:
        print("   Found GOOGLE_API_KEY in environment.")

    # --- Initialize LLM Once ---
    llm = None
    print(f"\n🤖 Initializing Google Gemini model: {TARGET_GOOGLE_MODEL}...")
    try:
        llm = ChatGoogleGenerativeAI(
            model=TARGET_GOOGLE_MODEL, # Hier wird die Konstante verwendet
            temperature=0,
            google_api_key=google_api_key,
            convert_system_message_to_human=True
        )
        # After successful initialization, we can confirm the actual model name used by the instance
        print(f"   Successfully initialized Google Gemini model: {llm.model}")
    except Exception as e:
        print(f"🔴 Error initializing Google Generative AI LLM: {e}")
        print(f"   Please ensure your API key is correct, the model '{TARGET_GOOGLE_MODEL}' is accessible by your key,")
        print("   your Google Cloud project has billing enabled, and quotas for this model are sufficient.")
        print("   Preview models may have very limited access or specific requirements.")
        return

    if not llm:
        print("🔴 Error: LLM could not be initialized. Exiting.")
        return

    if os.name == 'nt':
        asyncio.set_event_loop_policy(asyncio.WindowsProactorEventLoopPolicy())

    # --- Task Input Loop ---
    while True:
        print("\nExample tasks:")
        print("  - \"What are the latest Hugging Face models? Navigate to the models page, sort by trending, and list the top 5 model names and their likes.\"")
        print("  - \"Find the current price of Bitcoin on CoinMarketCap and report the price in USD.\"")
        print("  - \"Summarize the main points of the Wikipedia page for 'Artificial Intelligence'.\"")

        user_query = input("\n💬 Enter the task for the browser agent (or press Enter to quit): ").strip()

        if not user_query:
            print("\n👋 No task provided. Exiting the agent. Goodbye!")
            break

        await run_browser_agent_task(user_task=user_query, llm_instance=llm)

if __name__ == "__main__":
    print("🔧 Checking for Playwright browser installation...")
    print("   If this is your first time or you see errors, you might need to run:")
    print("   playwright install chromium --with-deps --no-shell")
    print("   (You might need to run `playwright install` without arguments first to see all options)")

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 User interrupted the program. Exiting.")
    except Exception as e:
        print(f"⚡ An unexpected error occurred in the main execution: {e}")
        traceback.print_exc()