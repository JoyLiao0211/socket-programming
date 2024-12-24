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
- response b->server
- response server-> a
- response b->a
- direct message a<->b

---

## Notes
- JSON keys are case-sensitive.
- Fields marked as `<string>` must be valid UTF-8 encoded strings.
- Responses always include a `code` field to indicate success or error.
- Additional fields may be included in responses for debugging purposes.
