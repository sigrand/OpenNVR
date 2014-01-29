//	JavaScript API 2.0 for Uppod 1+
//  http://uppod.ru/js

	// Events
	
	function uppodEvent(playerID,event) { 

		switch(event){
		
			case 'init': 
			
				break;
				
			case 'start': 
				
				break;
			
			case 'play': 
				
				break;
				
			case 'pause': 
				
				break;
				
			case 'stop': 
				
				break;
				
			case 'seek': 
							
				break;
				
			case 'loaded':
				
				break;
				
			case 'end':
				
				break;
				
			case 'download':
				
				break;
				
			case 'quality':
				
				break;
			
			case 'error':
				
				break;
					
			case 'ad_end':
				
				break;
				
			case 'pl':
				
				break;
			
			case 'volume':
				
				break;
		}
		
	}
	
	// Commands
	
	function uppodSend(playerID,com,callback) {
		document.getElementById(playerID).sendToUppod(com);
	}
	
	// Requests
	
	function uppodGet(playerID,com,callback) {
		return document.getElementById(playerID).getUppod(com);
	}

