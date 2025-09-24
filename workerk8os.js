//
// k8OS Cloudflare AI Provider - worker.js
// Updated to handle both text-only ("prompt") and multimodal ("messages") requests
//

export default {
  async fetch(request, env) {
    // CORS headers to allow k8o5.com to talk to your worker.
    const corsHeaders = {
      'Access-Control-Allow-Origin': 'https://k8o5.com',
      'Access-Control-Allow-Methods': 'POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Authorization, Content-Type',
    };

    // Handle CORS preflight requests
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: corsHeaders });
    }

    if (request.method !== 'POST') {
      return new Response('Expected POST', { status: 405, headers: corsHeaders });
    }

    // --- Authentication ---
    const apiKey = '1111'; // Your hardcoded API key
    const authHeader = request.headers.get('Authorization');

    if (!authHeader || authHeader !== `Bearer ${apiKey}`) {
      return new Response(JSON.stringify({ error: 'Unauthorized' }), { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } });
    }
    
    try {
      const requestBody = await request.json();
      
      // Get the model from the request, or default to the Llama 4 Scout model
      const model = requestBody.model || '@cf/meta/llama-4-scout-17b-16e-instruct'; 

      let inputs;

      // *** THIS IS THE KEY CHANGE ***
      // Check if the request body already contains the 'messages' array.
      // This is the new format for requests with images from k8OS.
      if (requestBody.messages) {
        inputs = {
          messages: requestBody.messages
        };
      } 
      // Fallback for older, text-only requests that just send a 'prompt'.
      else if (requestBody.prompt) {
        inputs = {
          messages: [{ role: 'user', content: requestBody.prompt }]
        };
      } 
      // If neither is present, it's a bad request.
      else {
        throw new Error('Request body must contain either "messages" or "prompt".');
      }

      // --- Run the AI model with the prepared inputs ---
      const aiResponse = await env.AI.run(model, inputs);
      
      // --- Return the AI's response directly to k8OS ---
      // The 'response' field in the k8OS app expects a text string.
      // Cloudflare's response for chat models is in aiResponse.response
      const responsePayload = {
          response: aiResponse.response || JSON.stringify(aiResponse)
      };

      return new Response(JSON.stringify(responsePayload), {
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });

    } catch (e) {
      return new Response(JSON.stringify({ error: e.message }), { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } });
    }
  },
};
