<?php
/**
 * phpWebHacks.php 1.8
 * This class is a powerful tool for HTTP scripting with PHP.
 * It simulates a web browser, only that you use it with lines of code
 * rather than mouse and keyboard.
 *
 * See the documentation at http://php-http.com/documentation
 * See the examples at http://php-http.com/examples
 *
 * Author  Nashruddin Amin - me@nashruddin.com
 * Edited by Exgibichi
 * License GPL
 * Website http://php-http.com
 */

class http
{ 
	private $_user_agent   = 'Opera/9.80 (Windows NT 5.1; U; ru) Presto/2.7.62 Version/12.00';
	private $_boundary 	   = '----PhPWebhACKs-RoCKs--';
	private $_currentUrl   = '';
	private $_useproxy 	   = false;
	private $_request_time = false;
	private $_useauth 	   = false;
	private $_changeType   = false;
	private $_cpanel 	   = false;
	private $_proxy_host   = '';
	private $_proxy_port   = '';
	private $_proxy_user   = '';
	private $_proxy_pass   = '';
	private $_usegzip 	   = false;
	private $_log 		   = false;
	private $_debugdir     = '.log';
	private $_debugnum     = 1;
	private $_delay 	   = 1;
	private $_head 		   = array();
	private $_body 		   = array();
	var $_cookies   	   = array();
	var $_context   	   = array();
	private $_addressbar   = '';
	private $_multipart    = false;
	private $_timestart    = 0;
	private $_bytes 	   = 0;
	private $_port 	   	   = 0;
	private $_requests 	   = 0;

	/**
	 * Constructor
	 */
	public function __construct()
	{
		$this->setDebug(false);

		/* check if zlib is available */
		if (function_exists('gzopen')) {
			$this->_usegzip = true;
		}

		/* start time */
		$this->_timestart = microtime(true);
	} 

	/**
	 * Destructor
	 */
	public function __destruct()
	{
		/* remove temporary file for gzip encoding */
		if (file_exists('tmp.gz')) {
			@unlink('tmp.gz');
		}

		/* get elapsed time and transferred bytes */
		$time  = sprintf("%02.1f", microtime(true) - $this->_timestart);
		$bytes = sprintf("%d", ceil($this->_bytes / 1024));

		/* log */
		if ($this->_log) {
			$fp = fopen("$this->_debugdir/headers.txt", 'a');
			fputs($fp, "------ Transferred " . $bytes . "kb in $time sec ------\r\n");
			fclose($fp);
		}
	} 

	/** 
	 * HEAD 
	 */
	public function head($url)
	{ 
		return $this->fetch($url, 'HEAD');
	} 

	/**
	 * GET
	 */
	public function get($url, $params = array()) { 
		
		if(!empty($params)) { $url .= '?'.http_build_query($params); }
		
		return $this->fetch($url, 'GET');
		
	} 

	/**
	 * POST
	 */
	public function post($url, $form = array(), $files = array())
	{ 
		return $this->fetch($url, 'POST', 10, $form, $files);
	} 

	/**
	 * Make HTTP request
	 */
	protected function fetch($url, $method, $maxredir = 10, $form = array(), $files = array())
	{
		/* convert to absolute if relative URL */
		$this->_currentUrl = $url = $this->getAbsUrl($url, $this->_addressbar);

		/* only http or https */
		if (substr($url, 0, 4) != 'http') return '';

		/* cache URL */
		$this->_addressbar = $url;

		/* build request */
		$reqbody = $this->getReqBody($form, $files);
		$reqhead = $this->getReqHead($url, $method, strlen($reqbody), empty($files) ? false : true);
		if(is_array($this->_changeType)) {
			$reqhead = preg_replace("/{$this->_changeType[0]}/i", $this->_changeType[1], $reqhead);
		}
		//print_r($reqhead);
		/* log request */
		if ($this->_log) {
			$this->logHttpStream($url, $reqhead, $reqbody);
		}

		/* parse URL and convert to local variables:
		$scheme, $host, $path */
		$parts = parse_url($url);

		if (!$parts) { 

			die("Invalid URL!\n");
		} else { 

			foreach($parts as $key=>$val) $$key = $val;
			
		} 
		
		if($this->_cpanel && !$this->_port) {

			$port = $this->_cpanel; 

		} elseif(!$this->_port && !$port) {

			$port = $scheme == 'https' ? 443 : 80;

		} else {
			$port = $this->_port;
		}		
		/* open connection */
		if ($this->_useproxy) {
		//echo 'connected to ',$this->_proxy_host,':', $this->_proxy_port,"\r\n";
			$fp = stream_socket_client('tcp://'.$this->_proxy_host.':'.$this->_proxy_port, $errno, $errstr, ($this->_request_time != 0 ? $this->_request_time : 30));
			
		} else  {
			if(!is_array($this->_context)) {
				$fp = stream_socket_client(($scheme=='https' ? 'ssl://'.$host : $host).':'.$port, $errno, $errstr, ($this->_request_time != 0 ? $this->_request_time : 30), STREAM_CLIENT_CONNECT, $this->_context);
			} else {
				$fp = stream_socket_client(($scheme=='https' ? 'ssl://'.$host : $host).':'.$port, $errno, $errstr, ($this->_request_time != 0 ? $this->_request_time : 30));
			}
		}
		
		/* always check */
		if (!$fp) {

			echo "Cannot connect to $host!<br/>\n";
			return 0;
			
		}
		
		/* proxy connect */
		if($this->_useproxy && ($scheme=='https' || isset($port))) {
			
			$res = '';
			fputs($fp, "\r\nCONNECT $host:".($port ? $port : 443)." HTTP/1.0\r\nUser-Agent: $this->_user_agent\r\n\r\n");
			
			while(true) {
				$tmp = $res.=fgets($fp, 100); 
				if(stristr($tmp,"\r\n\r\n")) { break; }
			}
			
			if(!stristr($res, 'HTTP/1.0 200 Connection Established')) { echo "NO SSL\r\n"; return; }
			if(!stream_socket_enable_crypto($fp, 1, STREAM_CRYPTO_METHOD_SSLv23_CLIENT)) { echo "NO SSL2\r\n"; return; }
			
		}
		
		/* send request & read response */
		//file_put_contents('add_req.txt', $reqhead.$reqbody."\r\n", FILE_APPEND);
		@fputs($fp, $reqhead.$reqbody);
		
		if($this->_request_time) {

			$sx = gettimeofday();
			
		}
		
		for($res=''; !feof($fp); $res.=@fgets($fp, 4096)) {

			if($this->_request_time) {

				$ex = gettimeofday();
				$time = (float)($ex['sec']-$sx['sec']);
				
				if($time>$this->_request_time) {

					break; 

				} 
				
			}
			
		} 
		
		fclose($fp);
		//file_put_contents('1222.txt',$res);
		$this->_requests++;
		//file_put_contents('add_resp.txt', $res."\r\n", FILE_APPEND);
		/* set delay between requests. behave! */
		sleep($this->_delay);

		/* transferred bytes */
		$this->_bytes += (strlen($reqhead)+ strlen($reqbody)+ strlen($res));

		/* get response header & body */
		list($reshead, $resbody) = explode("\r\n\r\n", $res, 2);

		/* convert header to associative array */
		$this->_head = $this->parseHead($reshead);

		/* return immediately if HEAD */
		if ($method == 'HEAD') { 

			if ($this->_log) $this->logHttpStream($url, $reshead, null);
			
			return $this->_head;
			
		}

		/* cookies */
		if (!empty($this->_head['Set-Cookie'])) {
			$this->saveCookies($this->_head['Set-Cookie'], $url);
		}

		/* referer */
		if ($this->_head['Status']['Code'] == 200) {
			$this->_referer = $url;
		}

		/* transfer-encoding: chunked */
		if (isset($this->_head['Transfer-Encoding']) && $this->_head['Transfer-Encoding'] == 'chunked') {
			$body = $this->joinChunks($resbody);
		} else {
			$body = $resbody;
		} 

		/* content-encoding: gzip */
		if (isset($this->_head['Content-Encoding']) && $this->_head['Content-Encoding'] == 'gzip') {
			@file_put_contents('tmp.gz', $body);
			$fp = @gzopen('tmp.gz', 'r');
			for($body = ''; !@gzeof($fp); $body.=@gzgets($fp, 4096)) {}
				@gzclose($fp);
		} 

		/* log response */
		if ($this->_log) {
			$this->logHttpStream($url, $reshead, $body);
		}

		/* cache body */
		array_unshift($this->_body, $body);

		/* redirects: 302 */
		if (isset($this->_head['Location']) && $maxredir > 0) {
			$maxredir--;
			$this->fetch($this->getAbsUrl($this->_head['Location'], $url), 'GET', $maxredir);
		}

		/* parse meta tags */
		$meta = $this->parseMetaTags($body);

		/* redirects: <meta http-equiv=refresh...> */
		if (isset($meta['http-equiv']['refresh']) && $maxredir > 0) { 
			list($delay, $loc) = explode(';', $meta['http-equiv']['refresh'], 2);
			$loc = substr(trim($loc), 4);
			if (!empty($loc) && $loc != $url) {
				$maxredir--;
				$this->fetch($this->getAbsUrl($loc, $url), 'GET', $maxredir--);
			}
		}

		/* get body and clear cache */
		$body = $this->_body[0];
		
		for($i = 1; $i < count($this->_body); $i++) {

			unset($this->_body[$i]);
			
		}

		return $body;
		
	} 

	/**
	 * Build request header
	 */
	protected function getReqHead($url, $method, $bodylen = 0, $sendfile = true)
	{
		/* parse URL elements to local variables:
		$scheme, $host, $path, $query, $user, $pass */
		$parts = parse_url($url);
		
		foreach($parts as $key=>$val) $$key = $val;

		/* setup path */
		$path = empty($path)  ? '/' : $path 
		.(empty($query) ? ''  : "?$query");

		/* request header */	
		if ($this->_useproxy) {
			$head = "$method $url HTTP/1.1\r\nHost: $this->_proxy_host\r\n";
		} else  {
			$head = "$method $path HTTP/1.1\r\nHost: $host\r\n";
		}

		/* cookies */
		$head .= $this->getCookies($url);

		/* content-type */
		if ($method == 'POST' && ($sendfile || $this->_multipart)) {
			$head .= "Content-Type: multipart/form-data; boundary=$this->_boundary\r\n";
		} elseif ($method == 'POST') {
			$head .= "Content-Type: application/x-www-form-urlencoded\r\n";
		}

		/* set the content length if POST */
		if ($method == 'POST') {
			$head .= "Content-Length: $bodylen\r\n";
		}

		/* basic authentication */
		if (!$this->_useproxy && $this->_useauth && !empty($this->_proxy_user) && !empty($this->_proxy_pass)) {
			$head .= "Authorization: Basic ". base64_encode("$this->_proxy_user:$this->_proxy_pass")."\r\n";
		}

		/* basic authentication for proxy */
		if ($this->_useproxy && !empty($this->_proxy_user) && !empty($this->_proxy_pass)) {
			$head .= "Authorization: Basic ". base64_encode("$this->_proxy_user:$this->_proxy_pass")."\r\n";
		}

		/* gzip */
		if ($this->_usegzip) {
			$head .= "Accept-Encoding: gzip\r\n";
		}

		/* make it like real browsers */
		if (!empty($this->_user_agent)) {
			$head .= "User-Agent: $this->_user_agent\r\n";
		}
		if (!empty($this->_referer)) {
			$head .= "Referer: $this->_referer\r\n";
		}
		
		if (!empty($this->_aheader)) {
			$head .= "$this->_aheader";
		}

		/* no pipelining yet */
		$head .= "Connection: Close\r\n\r\n";

		/* request header is ready */
		return $head;
	} 

	/**
	 * Build request body
	 */
	protected function getReqBody($form = array(), $files = array())
	{ 
		/* check for parameters */
		if (empty($form) && empty($files)) 
			return '';
		
		if(!is_array($form)) { return $form; }

		$body = '';
		$tmp  = array();

		/* only form available: x-www-urlencoded */
		if (!empty($form) &&  empty($files) && !$this->_multipart) { 
			foreach($form as $key=>$val)
				$tmp[] = $key .'='. rawurlencode(html_entity_decode($val));
			return implode('&', $tmp);
		} 

		/* form */
		foreach($form as $key=>$val) {
			$body .= "--$this->_boundary\r\nContent-Disposition: form-data; name=\"" . $key ."\"\r\n\r\n" . $val ."\r\n";
		}

		/* files */
		foreach($files as $key=>$val) { 
			if (!file_exists($val)) continue;
			$body .= "--$this->_boundary\r\n"
			. "Content-Disposition: form-data; name=\"" . $key . "\"; filename=\"" . basename($val) . "\"\r\n"
			. "Content-Type: " . $this->getMimeType($val) . "\r\n\r\n"
			. file_get_contents($val) . "\r\n";
		} 

		/* request body is ready! */
		return $body."--$this->_boundary--";
	} 

	/**
	 * convert response header to associative array
	 */
	protected function parseHead($str)
	{
		$lines = explode("\r\n", $str);

		list($ver, $code, $msg) = explode(' ', array_shift($lines), 3);
		$stat = array('Version' => $ver, 'Code' => $code, 'Message' => $msg);

		$head = array('Status' => $stat);

		foreach($lines as $line) { 
			list($key, $val) = explode(':', $line, 2);
			if ($key == 'Set-Cookie') {
				$head['Set-Cookie'][] = trim($val);
			} else {
				$head[$key] = trim($val);
			}
		} 

		return $head;
	} 

	/**
	 * Read chunked pages
	 */
	protected function joinChunks($str)
	{
		$CRLF = "\r\n";
		for($tmp = $str, $res = ''; !empty($tmp); $tmp = trim($tmp)) { 
			if (($pos = strpos($tmp, $CRLF)) === false) return $str;
			$len = hexdec(substr($tmp, 0, $pos));
			$res.= substr($tmp, $pos + strlen($CRLF), $len);
			$tmp = substr($tmp, $pos + strlen($CRLF) + $len);
		} 
		return $res;
	}
	
	function addCookies($cookie) {
		$this->saveCookies(array($cookie), $this->_currentUrl);
	}
	
	function exportCookies($file) {
		file_put_contents($file, json_encode($this->_cookies));
	}

	function importCookies($file) {
		$this->_cookies = array_merge($this->_cookies, json_decode(file_get_contents($file), 1));
	}

	/**
	 * Save cookies from server
	 */
	function saveCookies($headers, $url, $cookies = array())
	{
		if(empty($url) || !is_array($headers))
			return false;

		if($headers === false)
			return false;

		$output = parse_url(trim($url));

		if(empty($output["path"]))
			$output["path"] = "/";

		foreach($headers as $header)
		{
			$elements = explode(";", $header);
			$cookie = array(
				"secure" => 0,
				"httponly" => 0
				);

			foreach($elements as $element)
			{
				@list($name, $content) = explode("=", trim($element), 2);
				$name = trim($name);

				if(empty($name))
					continue;

				switch(strtolower($name))
				{
					case "domain":
					$cookie[strtolower($name)] = trim($content);
					case "path":
					$cookie[strtolower($name)] = trim($content);
					case "comment":
					case "expires":
					case "max-age":
					$cookie[$name] = trim($content);
					break;
					case "secure":
					$cookie[strtolower($name)] = 1;
					case "httponly":
					$cookie[strtolower($name)] = 1;
					break;
					default:
					$cookie_name = $name;
					$cookie[$name] = trim($content);
					break;
				}
			}

			if(empty($cookie["domain"]))
				$cookie["domain"] = $output["host"];

			if(empty($cookie["path"]))
				$cookie["path"] = $output["path"];

			if(!isset($cookies[$cookie["domain"]][$cookie["path"]][$cookie_name]))
				$cookies[$cookie["domain"]][$cookie["path"]][$cookie_name] = array();

			$cookies[$cookie["domain"]][$cookie["path"]][$cookie_name] = array_merge($cookies[$cookie["domain"]][$cookie["path"]][$cookie_name], $cookie);
		}

		foreach($cookies as $domain => $paths) {
			
			if(isset($this->_cookies[$domain])) {
				foreach($paths as $path => $cookie) {
					if(isset($this->_cookies[$domain][$path])) {
						$this->_cookies[$domain][$path] = array_merge($this->_cookies[$domain][$path], $cookie);
					} else {
						$this->_cookies[$domain][$path] = $cookie;
					}
				}
			} else {
				$this->_cookies[$domain] = $paths;
			}
		}
        //$this->_cookies = array_merge($this->_cookies, $cookies);
        //print_r($cookies);
        //print_r($this->_cookies);
		return $cookies;
	}

    /**
     * Get cookies for URL
     */
    function getCookies($url, $expires = false) {
    	//var_dump($url);
    	$cookies = $this->_cookies;
        //print_r($cookies);
    	if(empty($url) || !is_array($cookies))
    		return false;

    	$output = parse_url(trim($url));
        //print_r($output);
    	if(empty($output['path']))
    		$output['path'] = '/';

    	$host_length = strlen($output['host']);
    	$path_length = strlen($output['path']);
    	$scheme = $output['scheme'];
    	$return = null;
    	//print_r($cookies);
    	foreach($cookies as $domain => $contents) {
            //$result = $host_length - strlen($domain);
            //print_r($contents[0]);
           // print_r(array($output["host"], $domain));
            //break;
    		if(strstr($output['host'], $domain) /*|| $result > 0*/ || (strstr(".".$output['host'], $domain))) {
    			foreach($contents as $path => $cookie) {
    				if($path_length >= strlen($path) && substr($output['path'], 0, strlen($path)) == $path) {
    					foreach($cookie as $name => $value){
    						if($expires && $this->is_expired_cookie($value['expires']))
    							continue;

    						if($scheme == 'http' && $value['secure'] != 0 && $value['httponly'] != 0)
    							continue;

    						if($value[$name] == 'deleted')
    							continue;

    						$return .= $name . '=' . $value[$name] . "; ";
    					}
    				}
    			}
    		}
    	}
    	//print_r($return);
    	return $return != '' ? 'Cookie: '.trim($return)."\r\n" : '';
    }
    
    function is_expired_cookie($time)
    {
    	if(empty($time))
    		return false;

    	$time_conv = strtotime($time);
    	$time_now = time();

    	if(is_int($time_conv) && $time_conv >= $time_now)
    		return false;
    	return true;
    }

	/**
	 * Convert relative URL to absolute URL
	 */
	protected function getAbsUrl($loc, $parent)
	{ 
		/* parameters is required */
		if (empty($loc) && empty($parent)) return;

		$loc = str_replace('&amp;', '&', $loc);

		/* return if URL is abolute */
		if (parse_url($loc, PHP_URL_SCHEME) != '') return $loc;

		/* handle anchors and query's part */
		$c = substr($loc, 0, 1);
		if ($c == '#' || $c == '&') return "$parent$loc";

		/* handle query string */
		if ($c == '?') {
			$pos = strpos($parent, '?');
			if ($pos !== false) $parent = substr($parent, 0, $pos);
			return "$parent$loc";
		}

		/* parse URL and convert to local variables:
		$scheme, $host, $path */
		$parts = parse_url($parent);
		foreach ($parts as $key=>$val) $$key = $val;

		/* remove non-directory part from path */
		$path = preg_replace('#/[^/]*$#', '', $path);

		/* set path to '/' if empty */
		$path = preg_match('#^/#', $loc) ? '/' : $path;

		/* dirty absolute URL */
		$port = isset($port) ? ':'.$port : '';
		$abs = "$host$port$path/$loc";

		/* replace '//', '/./', '/foo/../' with '/' */
		while($abs = preg_replace(array('#(/\.?/)#', '#/(?!\.\.)[^/]+/\.\./#'), '/', $abs, -1, $count)) 
			if (!$count) break;

		/* absolute URL */
		return "$scheme://$abs";
	} 

	/**
	 * Convert meta tags to associative array
	 */
	protected function parseMetaTags($html) 
	{ 
		/* extract to </head> */
		if (($pos = strpos(strtolower($html), '</head>')) === false) { 
			return array();
		} else {
			$head = substr($html, 0, $pos);
		} 

		/* get page's title */
		preg_match("/<title>(.+)<\/title>/siU", $head, $m);
		if(isset($m[1]) && !empty($m[1])) {
			$meta = array('title' => $m[1]);
		} else {
			$meta = array();
		}
		/* get all <meta...> */
		preg_match_all('/<meta\s+[^>]*name\s*=\s*[\'"][^>]+>/siU', $head, $m);
		foreach($m[0] as $row) { 
			preg_match('/name\s*=\s*[\'"](.+)[\'"]/siU', $row, $key);
			preg_match('/content\s*=\s *[\'"](.+)[\'"]/siU', $row, $val);
			if (!empty($key[1]) && !empty($val[1]))
				$meta[$key[1]] = $val[1];
		} 

		/* get <meta http-equiv=refresh...> */
		preg_match('/<meta[^>]+http-equiv\s*=\s*[\'"]?refresh[\'"]?[^>]+content\s*=\s*[\'"](.+)[\'"][^>]*>/siU', $head, $m);
		if (!empty($m[1])) {
			$meta['http-equiv']['refresh'] = preg_replace('/&#0?39;/', '', $m[1]);
		} 
		return $meta;
	} 

	/**
	 * Convert form to associative array
	 */
	public function parse_form($name_or_id, $str = '')
	{ 
		if (empty($str) && empty($this->_body[0])) 
			return array();
		$action = '';
		$body = empty($str) ? $this->_body[0] : $str;

		/* extract the form */
		$re = '(<form[^>]+(id|name)\s*=\s*(?(?=[\'"])[\'"]'.$name_or_id.'[\'"]|\b'.$name_or_id.'\b)[^>]*>.+<\/form>)';
		if (!preg_match("/$re/siU", $body, $form)) 
			return array();

		/* check if enctype=multipart/form-data */
		if (preg_match('/<form[^>]+enctype[^>]+multipart\/form-data[^>]*>/siU', $form[1], $a))
			$this->_multipart = true;
		else 
			$this->_multipart = false;

		/* get form's action */
		preg_match('/<form[^>]+action\s*=\s*(?(?=[\'"])[\'"]([^\'"]+)[\'"]|([^>\s]+))[^>]*>/si', $form[1], $a);
		$action = empty($a[1]) ? html_entity_decode($a[2]) : html_entity_decode($a[1]);

		/* select all <select..> with default values */
		$re = '<select[^>]+name\s*=\s*(?(?=[\'"])[\'"]([^>]+)[\'"]|\b([^>]+)\b)[^>]*>'
		. '.+value\s*=\s*(?(?=[\'"])[\'"]([^>]+)[\'"]|\b([^>]+)\b)[^>]+\bselected\b'
		. '.+<\/select>';
		preg_match_all("/$re/siU", $form[1], $a);

		foreach($a[1] as $num=>$key) {
			$val = $a[3][$num];
			if ($val == '') $val = $a[4][$num];
			if ($key == '') $key = $a[2][$num];
			$res[$key] = html_entity_decode($val);
		} 

		/* get all <input...> */
		preg_match_all('/<input([^>]+)\/?>/siU', $form[1], $a);

		/* convert to associative array */
		foreach($a[1] as $b) { 
			preg_match_all('/([a-z]+)\s*=\s*(?(?=[\'"])[\'"]([^"]+)[\'"]|\b(.+)\b)/siU', trim($b), $c);
			
			$element = array();
			
			foreach($c[1] as $num=>$key) {
				$val = $c[2][$num];
				if ($val == '') $val = $c[3][$num];
				$element[$key] = $val;
			}
			
			$type = strtolower($element['type']);
			
			/* only radio or checkbox with default values */
			if ($type == 'radio' || $type == 'checkbox') 
				if (!preg_match('/\s+\bchecked\b/', $b)) continue;

			/* remove buttons and file */	
			if ($type == 'file' || $type == 'submit' || $type == 'reset' || $type == 'button') 
				continue;
			
			/* remove unnamed elements */
			if (isset($element['id']) && $element['name'] == '' && $element['id'] == '')
				continue;
			
			/* cool */
			$key = !isset($element['name']) || $element['name'] == '' ? $element['id'] : $element['name'];
			$res[$key] = isset($element['value']) ? html_entity_decode($element['value']) : '';
		}

		return array($res, $action);
	} 

	/**
	 * Get mime type for a file
	 */
	protected function getMimeType($filename)
	{ 
		/* list of mime type. add more rows to suit your need */
		$mimetypes = array(
			'jpg'  => 'image/jpeg',
			'jpe'  => 'image/jpeg',
			'jpeg' => 'image/jpeg',
			'gif'  => 'image/gif',
			'png'  => 'image/png',
			'tiff' => 'image/tiff',
			'html' => 'text/html',
			'txt'  => 'text/plain',
			'pdf'  => 'application/pdf',
			'zip'  => 'application/zip'
			);

		/* get file extension */
		preg_match('#\.([^\.]+)$#', strtolower($filename), $e);

		/* get mime type */
		foreach($mimetypes as $ext=>$mime)
			if ($e[1] == $ext) return $mime;

		/* this is the default mime type */
		return 'application/octet-stream';
	} 

	/**
	 * Log HTTP request/response
	 */
	protected function logHttpStream($url, $head, $body)
	{ 
		/* open log file */
		if (($fp = @fopen("$this->_debugdir/headers.txt", 'a')) == false) return;

		/* get method */
		$m = substr($head, 0, 4);

		/* append the requested URL for HEAD, GET and POST */
		if ($m == 'HEAD' || $m == 'GET ' || $m == 'POST')
			$head = str_repeat('-', 90) . "\r\n$url\r\n\r\n" . trim($head);

		/* header */
		@fputs($fp, trim($head)."\r\n\r\n");

		/* request body */
		if ($m == 'POST' &&  strpos($head, 'Content-Length: ') !== false) {
			/* skip binary contents */
			$find = 'Content-Type: \s*([^\s]+)\r\n\r\n(.+)\r\n';
			$repl = "Content-Type: $1\r\n\r\n <... File contents ...>\r\n";
			$body = preg_replace('/'.$find .'/siU', $repl, $body);

			@fputs($fp, "$body\r\n\r\n");
		} 

		/* response body */
		if (substr($head, 0, 7) == 'HTTP/1.' && strpos($head, 'text/html') !== false && !empty($body)) {
			$tmp = "$this->_debugdir/" . $this->_debugnum++ . '.html';
			@file_put_contents($tmp, $body);
			@fputs($fp, "<... See page contents in $tmp ...>\r\n\r\n");
		}

		@fclose($fp);
	} 

	public function setDebug($bool)
	{
		$this->_log = $bool;

		if (!$this->_log) return;

		/* create directory */
		if (!is_dir($this->_debugdir)) { 
			mkdir($this->_debugdir);
			chmod($this->_debugdir, 0644);
		}

		/* empty debug directory */
		$items = scandir($this->_debugdir);
		foreach($items as $item) { 
			if ($item == '.' || $item == '..') continue;
			unlink("$this->_debugdir/$item");
		}
	} 

	/**
	 * Set proxy
	 */
	public function setProxy($host, $port, $user = '', $pass = '')
	{
		$this->_proxy_host = $host;
		$this->_proxy_port = $port;
		$this->_proxy_user = $user;
		$this->_proxy_pass = $pass;
		$this->_useproxy   = true;
	} 
	
	/**
	 * Set basic auth
	 */
	public function setAuth($user = '', $pass = '', $cpanel = '')
	{

		$this->_proxy_user = $user;
		$this->_proxy_pass = $pass;
		$this->_useauth   = true;
		$this->_cpanel   = $cpanel;
	} 

	/**
	 * Set delay between requests
	 */
	public function setInterval($sec)
	{ 
		if (!preg_match('/^\d+$/', $sec) || $sec <= 0) {
			$this->_delay = 1;
		} else { 
			$this->_delay = $sec;
		}
	} 

	/**
	 * Assign a name for this HTTP client
	 */
	public function setUa($ua)
	{
		$this->_user_agent = $ua;
	}
	
	/**
	 * Set timeout
	 */
	public function setTimeout($t) {

		$this->_request_time = $t;
		
	}
	
	public function free() {

		$this->_body = '';
		$this->_head = '';
		$this->_cookies = '';
		$this->_addressbar = '';

	}
	
	public function proxyConnect($fp,$host,$port) {
		
		fputs($fp, "\r\nCONNECT ".$host.':'.($port ? $port : 443)." HTTP/1.0\r\nUser-Agent: $this->_user_agent\r\n");
		//file_put_contents('123.txt',"\r\nCONNECT ".$host.':'.($port ? $port : 443)." HTTP/1.0\r\nUser-Agent: $this->_user_agent\r\n");
		for($res=''; !feof($fp); $res.=fgets($fp, 4096)) {

			if($this->_request_time) {

				$ex = gettimeofday();
				$time = (float)($ex['sec']-$sx['sec']);
				
				if($time>$this->_request_time) {

					break; 

				} 
				
			}
			
		}
		
		//print_r($res);
		
	}
	
	public function addHeader($hx) {

		$this->_aheader .= $hx."\r\n";
		
	}
	
	public function getHeader($hx = '') {

		return $hx == '' ? $this->_head : $this->_head[$hx];
		
	}
	
	public function setPort($port) {

		$this->_port = $port;

	}
	
	public function setRef($url) {

		$this->_referer = $url;

	}
	
	public function getUrl($url) {
		
		return $this->getAbsUrl($url, $this->_addressbar);
		
	}
	
	public function getCurrentUrl() {

		return $this->_addressbar;
		
	}
	
	public function getRequests() {

		return $this->_requests;
		
	}
	
	public function setInterface($i) {
		$opts = array(
			'socket' => array(
				'bindto' => $i.':0',
				)
			);
		$this->_context = stream_context_create($opts);
	}
	
	public function setType($type) {
		$this->_changeType[0] = "Content-Type: (.*?)\r\n";
		$this->_changeType[1] = "Content-Type: $type\r\n";
	}

	public function unsetType() {
		$this->_changeType = false;
	}

}
?>