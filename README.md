# Xmodem
 Internet Transportation Protocol

File are transport by a state-base protocol that ensures:

- Receive multiple files in parallel (without using multi-threading or multi-processing!).
- Package lost detection
- Corrupted data detection
- Handle lost and corrupt data packages
- Lossless transfer
- Light weight



## Usage

To compile:

```bash
make
```



To transport a file:

- start the server first

  ```bash
  hacker@ubuntu$ ./xmodemserver
  listening on port 53862
  ```

  it will prompt the port used for connection.

- send the file

  ```bash
  hacker@ubuntu$ ./client localhost 53862 <filename>
  ```

  

File received will be stored under ```filestore/```



## Disclaimer

This program is use for learning purpose. You have permission to read and copy for personal or business purpose, but you can not use it for school assignment! Read LICENSE.
