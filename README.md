# RedFlix
Operating Systems project by José Julio Suárez

## Compilation
Execute `$ make` on a Bash terminal.

## Execution
1. Execute `$ ./server` on a Bash terminal.
2. Execute `$ ./visor` on a different Bash terminal.

## Usage
The visor program will prompt for a server IP address to connect.
Enter the IP address of the machine running the server, or `localhost` if
it is running on the same machine and press Enter.

The visor will then launch a Controls window, where you must press Start+S
to begin the streaming output on the visor.

Once the data starts flowing, you can change bitrate to Low Definition (Shift+L),
Medium Definition (Shift+M) or High Definition (Shift+H). By default, streaming
will start at High Definition.

At any time during the streaming, you can press Shift+Q to stop and disconnect
from the server. 

When the streaming is completed, the visor will terminate.
