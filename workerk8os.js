/**
 * A Cloudflare Worker that acts as a rewriting CORS proxy for k8OS.
 * It takes a `url` query parameter, fetches it, rewrites all links in the
 * HTML to also use the proxy, and returns the response with CORS headers.
 *
 * @example
 * fetch('https://your-worker.workers.dev/?url=https://example.com')
 */

// This class rewrites HTML attributes to ensure all links and resources
// are also routed through the proxy.
class AttributeRewriter {
  constructor(proxyOrigin, targetUrl) {
    this.proxyOrigin = proxyOrigin;
    this.targetUrl = targetUrl;
  }

  element(element) {
    const attributesToRewrite = ['href', 'src', 'action'];

    for (const attribute of attributesToRewrite) {
      const value = element.getAttribute(attribute);
      if (value) {
        try {
          // Resolve the URL relative to the target page's URL to get an absolute URL.
          const absoluteUrl = new URL(value, this.targetUrl).toString();
          // Rewrite the attribute to point back to the proxy.
          element.setAttribute(attribute, `${this.proxyOrigin}/?url=${encodeURIComponent(absoluteUrl)}`);
        } catch (e) {
          // Ignore invalid URLs.
          console.error(`Failed to rewrite URL: ${value}`, e);
        }
      }
    }
  }
}

export default {
  async fetch(request) {
    // For production, you should restrict this to your actual k8o5 domain
    // for better security, e.g., 'https://k8o5.com'.
    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, HEAD, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization',
      'Access-Control-Allow-Credentials': 'true',
    };

    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: corsHeaders });
    }

    const url = new URL(request.url);
    const targetUrl = url.searchParams.get('url');

    if (!targetUrl) {
      return new Response('Error: The "url" query parameter is required.', { status: 400, headers: corsHeaders });
    }

    try {
      new URL(targetUrl);
    } catch (e) {
      return new Response('Error: Invalid "url" query parameter provided.', { status: 400, headers: corsHeaders });
    }

    try {
      const response = await fetch(targetUrl, {
        redirect: 'follow',
        headers: { 'User-Agent': 'k8OS-Proxy/1.1' }
      });
      
      const contentType = response.headers.get('content-type') || '';

      // Only rewrite HTML content. Pass through other content types like images directly.
      if (!contentType.includes('text/html')) {
        const newResponse = new Response(response.body, response);
        for (const key in corsHeaders) {
          newResponse.headers.set(key, corsHeaders[key]);
        }
        return newResponse;
      }

      // Initialize the rewriter with the proxy's origin and the target page's URL.
      const rewriter = new HTMLRewriter()
        .on('a, link, img, script, iframe, form', new AttributeRewriter(url.origin, targetUrl));
      
      const rewrittenResponse = rewriter.transform(response);

      // Clone the response to set new headers.
      const finalResponse = new Response(rewrittenResponse.body, rewrittenResponse);
      for (const key in corsHeaders) {
        finalResponse.headers.set(key, corsHeaders[key]);
      }
      
      // Remove problematic headers from the original response.
      finalResponse.headers.delete('Content-Security-Policy');
      finalResponse.headers.delete('X-Frame-Options');

      return finalResponse;

    } catch (e) {
      return new Response(`Error fetching or rewriting the target URL: ${e.message}`, {
        status: 500,
        headers: corsHeaders
      });
    }
  },
};
