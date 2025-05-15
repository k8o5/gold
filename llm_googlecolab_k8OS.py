# --- Step 1: Install necessary packages ---
print("Installing required packages (Flask, ngrok, transformers, torch, bitsandbytes, accelerate)...")
!pip install Flask flask-cors pyngrok transformers torch bitsandbytes accelerate --quiet
print("Packages installed.")

import gc
from flask import Flask, request, jsonify
from flask_cors import CORS
from pyngrok import conf, ngrok
import os
import threading
import torch
import shutil

# --- Step 2: User Configuration - !! MODIFY THESE TWO LINES !! ---
NGROK_AUTH_TOKEN = '2nA2jy4xiGU15vKvtzIsiYMXKnK_2ZRXX9KHB7dTdKZvx7m3A'
ENDPOINT_API_KEY = '1234'
# --- End User Configuration ---

PORT = 5000

current_model_llm = None
current_tokenizer_llm = None
current_generator_llm = None
flask_thread = None
ngrok_tunnel_obj = None

if NGROK_AUTH_TOKEN == 'YOUR_NGROK_AUTHTOKEN_HERE' or not NGROK_AUTH_TOKEN:
    print("ERROR: NGROK_AUTH_TOKEN is not set. Please replace placeholder.")
    raise ValueError("NGROK_AUTH_TOKEN not configured.")
if ENDPOINT_API_KEY == 'YOUR_CHOSEN_SECRET_API_KEY_HERE' or not ENDPOINT_API_KEY:
    print("ERROR: ENDPOINT_API_KEY is not set. Please replace placeholder.")
    raise ValueError("ENDPOINT_API_KEY not configured.")

conf.get_default().auth_token = NGROK_AUTH_TOKEN

def clear_gpu_and_python_memory():
    global current_generator_llm, current_model_llm, current_tokenizer_llm
    print("\nAttempting to clear resources...")
    if current_generator_llm:
        del current_generator_llm
        current_generator_llm = None
        print("  - Deleted generator pipeline.")
    if current_model_llm:
        try:
            if hasattr(current_model_llm, 'cpu'):
                current_model_llm.cpu()
        except Exception as e:
            print(f"    Note: Error moving model to CPU before delete: {e}")
        del current_model_llm
        current_model_llm = None
        print("  - Deleted model.")
    if current_tokenizer_llm:
        del current_tokenizer_llm
        current_tokenizer_llm = None
        print("  - Deleted tokenizer.")
    
    gc.collect()
    print("  - Ran Python garbage collection.")
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
        print("  - Emptied PyTorch CUDA cache.")
    print("Resource clearing attempt complete.")

def clear_huggingface_disk_cache():
    cache_dir = os.path.expanduser("~/.cache/huggingface")
    if os.path.exists(cache_dir):
        try:
            print(f"\nWARNING: Clearing Hugging Face disk cache at {cache_dir}.")
            print("This will force ALL models to be re-downloaded on next load.")
            confirm = input("Are you sure you want to proceed? (yes/no): ").lower()
            if confirm == 'yes':
                shutil.rmtree(cache_dir)
                os.makedirs(cache_dir, exist_ok=True)
                print("Hugging Face disk cache cleared.")
            else:
                print("Disk cache clearing aborted by user.")
        except Exception as e:
            print(f"Error clearing Hugging Face disk cache: {e}")
    else:
        print("Hugging Face disk cache directory not found.")

app = Flask(__name__)
CORS(app)

@app.route('/generate', methods=['POST'])
def generate_text_from_k8os():
    global current_generator_llm

    auth_header = request.headers.get('Authorization')
    if not auth_header or not auth_header.startswith('Bearer '):
        return jsonify({"error": "Unauthorized: Missing or malformed Bearer token"}), 401
    token_from_k8os = auth_header.split(' ', 1)[1]
    if token_from_k8os != ENDPOINT_API_KEY:
        return jsonify({"error": "Unauthorized: Invalid API Key"}), 401

    try:
        data = request.get_json()
        if not data or 'prompt' not in data:
            return jsonify({"error": "Bad Request: Missing 'prompt' in JSON payload"}), 400
        
        prompt_from_k8os = data.get('prompt')
        max_tokens_requested = data.get('max_tokens', 75) 
        temperature = data.get('temperature', 0.7)

        llm_output_text = ""

        if current_generator_llm:
            try:
                instruct_prompt = prompt_from_k8os
                print(f"\n[LLM Request] Prompt to '{current_generator_llm.model.name_or_path}': \"{instruct_prompt[:100]}...\"")
                print(f"             Requested max_new_tokens: {max_tokens_requested}, Temp: {temperature}")

                generated_sequences = current_generator_llm(
                    instruct_prompt,
                    max_new_tokens=max_tokens_requested,
                    num_return_sequences=1,
                    pad_token_id=current_generator_llm.tokenizer.pad_token_id,
                    eos_token_id=current_generator_llm.tokenizer.eos_token_id,
                    temperature=temperature if temperature > 0 else None,
                    do_sample=True if temperature > 0.01 else False,
                )
                if generated_sequences and generated_sequences[0] and 'generated_text' in generated_sequences[0]:
                    full_generated_text = generated_sequences[0]['generated_text']
                    if full_generated_text.startswith(instruct_prompt) and len(instruct_prompt) < len(full_generated_text):
                        llm_output_text = full_generated_text[len(instruct_prompt):].strip()
                    else:
                        llm_output_text = full_generated_text.strip()
                    print(f"[LLM Response] Generated: \"{llm_output_text[:200]}...\"")
                else:
                    llm_output_text = "LLM generated an empty or unexpected response structure."
            except Exception as llm_e:
                print(f"ERROR during LLM generation: {llm_e}")
                llm_output_text = f"Error with LLM: {str(llm_e)[:150]}"
        else:
            llm_output_text = (f"LLM is not currently loaded on this Colab server. "
                               f"Received: Prompt='{prompt_from_k8os}'.")
        
        return jsonify({"response": llm_output_text})

    except Exception as e:
        print(f"ERROR in /generate endpoint: {e}")
        return jsonify({"error": "Internal Server Error", "details": str(e)}), 500

def start_server_and_ngrok(hf_model_id, trust_remote_code):
    global current_model_llm, current_tokenizer_llm, current_generator_llm, flask_thread, ngrok_tunnel_obj
    
    clear_gpu_and_python_memory()

    print(f"\nInitializing LLM: '{hf_model_id}'. Trust remote code: {trust_remote_code}")
    print("This may take a SIGNIFICANT AMOUNT OF TIME...")

    device_map_config = "auto" if torch.cuda.is_available() else {"": "cpu"}
    if torch.cuda.is_available():
        print(f"CUDA (GPU) is available! Device: {torch.cuda.get_device_name(0)}")
    else:
        print("CUDA (GPU) not available. LLM will run on CPU (VERY SLOW for large models).")

    try:
        from transformers import AutoTokenizer, AutoModelForCausalLM, BitsAndBytesConfig, pipeline, set_seed

        quantization_config = BitsAndBytesConfig(load_in_8bit=True)
        
        print(f"Loading tokenizer for '{hf_model_id}'...")
        current_tokenizer_llm = AutoTokenizer.from_pretrained(hf_model_id, trust_remote_code=trust_remote_code)
        print("Tokenizer loaded.")

        print(f"Loading model '{hf_model_id}' with 8-bit quantization...")
        current_model_llm = AutoModelForCausalLM.from_pretrained(
            hf_model_id,
            quantization_config=quantization_config,
            device_map=device_map_config,
            trust_remote_code=trust_remote_code,
            torch_dtype=torch.float16
        )
        print("Model loaded.")

        current_generator_llm = pipeline(
            'text-generation', model=current_model_llm, tokenizer=current_tokenizer_llm
        )

        if current_generator_llm.tokenizer.pad_token is None:
            if current_generator_llm.tokenizer.eos_token is not None:
                current_generator_llm.tokenizer.pad_token = current_generator_llm.tokenizer.eos_token
                print(f"Set pad_token to eos_token for '{hf_model_id}' tokenizer.")
            else:
                current_generator_llm.tokenizer.add_special_tokens({'pad_token': '[PAD]'})
                current_model_llm.resize_token_embeddings(len(current_generator_llm.tokenizer))
                print(f"Added new [PAD] token for '{hf_model_id}' tokenizer.")
        set_seed(42)
        running_on = "GPU" if torch.cuda.is_available() and next(current_model_llm.parameters()).is_cuda else "CPU"
        print(f"LLM ({hf_model_id}) pipeline configured successfully (quantized). Running on: {running_on}")

    except Exception as e:
        print(f"ERROR: Could not load LLM pipeline for {hf_model_id}: {e}")
        current_generator_llm = None
        return False

    try:
        print("\nChecking for existing ngrok tunnels...")
        for tunnel in ngrok.get_tunnels():
            addr_str = tunnel.config.get('addr', '')
            if isinstance(addr_str, str) and addr_str.endswith(f":{PORT}"):
                print(f"  Closing existing tunnel: {tunnel.public_url} -> {addr_str}")
                ngrok.disconnect(tunnel.public_url)
                try: ngrok.kill()
                except: pass
        
        print(f"Starting new ngrok tunnel for port {PORT}...")
        ngrok_tunnel_obj = ngrok.connect(PORT, name=f"k8os-{hf_model_id.split('/')[-1][:10]}")
        public_ngrok_url_base = ngrok_tunnel_obj.public_url
        k8os_endpoint_url = f"{public_ngrok_url_base}/generate"

        print("\n" + "="*70)
        print(f"SERVER READY for model: {hf_model_id}")
        print("IMPORTANT: k8OS CONFIGURATION DETAILS")
        print(f"  k8OS Colab Endpoint URL: {k8os_endpoint_url}")
        print(f"  k8OS Endpoint API Key:    {ENDPOINT_API_KEY}")
        print("  (Update k8OS 'System Settings' and 'API Key' app accordingly)")
        print("="*70)

        if flask_thread is None or not flask_thread.is_alive():
            flask_thread = threading.Thread(target=lambda: app.run(host='0.0.0.0', port=PORT, debug=False, use_reloader=False))
            flask_thread.daemon = True
            flask_thread.start()
            print("Flask server thread started/restarted.")
        else:
            print("Flask server thread already running.")
        return True

    except Exception as e:
        print(f"ERROR starting ngrok or Flask for {hf_model_id}: {e}")
        if ngrok_tunnel_obj:
            try: ngrok.disconnect(ngrok_tunnel_obj.public_url); ngrok.kill()
            except: pass
        return False

if __name__ == '__main__':
    print("\n--- k8OS Colab AI Endpoint Server (Dynamic Model Loader) ---")
    print("Ensure Colab Runtime is set to GPU for best performance with large models.")

    keep_running_script = True
    while keep_running_script:
        print("\n--- Main Menu ---")
        hf_model_id_to_load = input("Enter Hugging Face Model ID (or 'clear_cache', 'quit'): ").strip()

        if not hf_model_id_to_load:
            print("No input. Please enter a model ID, 'clear_cache', or 'quit'.")
            continue
        
        if hf_model_id_to_load.lower() == 'quit':
            keep_running_script = False
            print("Exiting script.")
            break
        
        if hf_model_id_to_load.lower() == 'clear_cache':
            clear_huggingface_disk_cache()
            continue

        trust_remote_code = input(f"Does model '{hf_model_id_to_load}' require 'trust_remote_code=True'? (yes/no, default: no): ").strip().lower() == 'yes'
        
        if start_server_and_ngrok(hf_model_id_to_load, trust_remote_code):
            print(f"\nServer is running with model '{hf_model_id_to_load}'.")
            print("The Flask server will continue to run in the background.")
            print("To load a NEW model or stop: Interrupt this cell (CTRL+C or stop button), then re-run or choose 'quit'.")
            
            try:
                while True: 
                    if flask_thread and flask_thread.is_alive():
                        threading.Event().wait(timeout=5) 
                    else:
                        print("Flask thread appears to have stopped unexpectedly.")
                        break
            except KeyboardInterrupt:
                print("\nCTRL+C received in main loop. Shutting down this session.")
                keep_running_script = False 
            break 
        else:
            print(f"Failed to start server with model '{hf_model_id_to_load}'. Please check errors.")
            try_again = input("Try another model? (yes/no): ").strip().lower()
            if try_again != 'yes':
                keep_running_script = False

    print("\nCleaning up before exit...")
    if ngrok_tunnel_obj:
        try:
            print(f"Disconnecting ngrok tunnel: {ngrok_tunnel_obj.public_url}")
            ngrok.disconnect(ngrok_tunnel_obj.public_url)
        except Exception as e:
            print(f"Error disconnecting ngrok tunnel: {e}")
    try:
        print("Killing ngrok process...")
        ngrok.kill()
    except Exception as e:
        print(f"Error killing ngrok process: {e}")
    
    clear_gpu_and_python_memory()
    print("Script finished.")
