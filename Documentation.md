# Server and Client Documentation

## Overview
This document describes the communication protocol between the server and client, including the format for requests and responses, and the possible error codes.

---

## Error Codes
The following codes are used in responses to indicate the outcome of a request:

| Code | Message                               |
|------|---------------------------------------|
| 0    | Success                               |
| 1    | Already logged in                     |
| 2    | Incorrect password                    |
| 3    | Username not found                    |
| 4    | Username already exists               |
| 5    | Invalid command                       |
| 6    | Not logged in                         |
| 7    | Recipient not found                   |
| 8    | Recipient rejected connection         |
| 9    | Cannot connect to peer                |

---

## Requests and Responses

### 1. Login
**Request:**
```json
{
    "type": "Login",
    "username": "<string>",
    "password": "<string>"
}
```

**Response:**
```json
{
    "type": "Login",
    "code": <int>
}
```

### 2. Logout
**Request:**
```json
{
    "type": "Logout"
}
```

**Response:**
```json
{
    "type": "Logout",
    "code": <int>
}
```

### 3. Register
**Request:**
```json
{
    "type": "Register",
    "username": "<string>",
    "password": "<string>"
}
```

**Response:**
```json
{
    "type": "Register",
    "code": <int>
}
```

### 4. Online Users
**Request:**
```json
{
    "type": "OnlineUsers"
}
```

**Response:**
```json
{
    "type": "OnlineUsers",
    "code": 0,
    "users": [<string>, <string>, ...]
}
```

### 5. Send Message
**Request:**
```json
{
    "type": "SendMessage",
    "username": "<string>",
    "message": "<string>"
}
```

**Response:**
```json
{
    "type": "SendMessage",
    "code": <int>
}
```

**NewMessage:**
```json
{
    "type": "NewMessage",
    "from": <string>,
    "message": <string>
}
```

### 6. Direct Connect
**1. Request (a -> server):**
```json
{
    "type": "DirectConnect",
    "username": "<string>",
    "IP": "<string>",
    "port": <int>,
    "passcode": "<string>"
}
```

**2. Request (server -> b):**
```json
{
    "type": "DirectConnectRequest", 
    "username": "<string>",
    "IP": "<string>",
    "port": <int>,
    "passcode": "<string>"
}
```

**3. Response (b -> server):**
```json
{
    "type": "DirectConnectRequest",
    "code": <int>
}
```

**4. Response (server -> a):**
```json
{
    "type": "DirectConnect",
    "code": <int>
}
```



**5. Response (b -> a):** ==BUG here==
```json
{
    "type": "DirectConnect",
    "passcode": <string>
}
```


**6. Direct message(a<->b):**
```json
{
    "type": "DirectMessage"
    "message": <string>
}
```

#### workflow

- request a->server
- request server->b
- b send a response to a directly
- response b->server
- response server-> a
- a reads the response sent directly from b
- direct message a<->b

---

### 7. File transfer

1. **A asks if they can send a file to B**

```json
{
    "type": "TransferFileRequest",
    "filename“: <string>,//optional
}
```

2. **B responds to A's request**

```json
{
    "type": "TransferFileResponse",
    "accept": <int> //1 for ok, 0 for no
}
```

3. **A sends the file to B**

no json here

#### workflow
- scenario: A wants to send a file to B
- A and B establish direct connection and use direct connection to transfer files
1. A: can i send a file to you?
2. B: ok
3. A: send the file

### 8. Audio streaming

1. **audio request from client:**

```json
{
    "type": "AudioRequest",
    "filename": <string>
}
```
- if filename is empty then request for audio list

2. **audio list response from server**

```json
{
    "type": "AudioList",
    "code": <int>,
    "files": [
        <string>,
        <string>,
        ...
    ]
}
```

3. **audio metadata response from server**

```json
{
    "type": "AudioResponse", 
    "code": <int>,
    "rate": <int>,
    "channels": <int>
}
```

4.**audio data response from server**

```json
{
    "type": "AudioData",
    "data": vector<char>,
    "pos": <int>,
    "end": <int>
}
```

### 9. Video streaming

1. **video request from client**

```json
{
    "type": "VideoRequest",
    "filename": <string>
}
```
- request for list: filename = ""

2. **video list response from server**

```json
{
    "type": "VideoList",
    "code": <int>,
    "files": [ <string>, <string>, ...]
}
```

3. **video data response**

```json
{
    "type": "VideoResponse",
    "code": <int>,
    "data": vector<char>,
    "start": <int>,
    "end": <int>
}
```
- start and end are used as bool
- first json of a video must have start = 1, end = 0, data = {}
- all json containing data must have start = 0, end = 0, data = {}
- last json of a video must have start = 0, end = 1, data = {}


## Notes
- JSON keys are case-sensitive.
- Fields marked as `<string>` must be valid UTF-8 encoded strings.
- Responses always include a `code` field to indicate success or error.
- Additional fields may be included in responses for debugging purposes.
