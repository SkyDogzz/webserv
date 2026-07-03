# webserv

Architecture globale du projet `webserv` et flux principal d'une requête HTTP.

```mermaid
flowchart TD
    A["main.cpp"] --> B["Load config"]
    B --> C["Config"]
    C --> D["validate config"]
    D --> E["WebServer singleton"]
    E --> F["apply config"]
    F --> G["run server"]
    G --> H["EventLoop"]
    H --> I["ListeningSocket"]
    I --> J["accept client"]
    J --> K["Connection"]
    K --> L["read socket"]
    L --> M["HttpRequestParser"]
    M --> N{"complete and valid?"}
    N -- "no" --> O["HTTP 400"]
    N -- "yes" --> P["Router"]
    P --> Q["ServerConfig"]
    P --> R["LocationConfig"]
    Q --> S["RequestContext"]
    R --> S
    S --> T["root / index / autoindex"]
    S --> U["redirect / allow / cgi / errors / upload"]
    S --> V["StaticHandler"]
    V --> W{"GET / HEAD / POST / DELETE"}
    W -- "unsupported" --> O
    W -- "supported" --> X["HttpResponse"]
    X --> Y["serialize response"]
    Y --> Z["write socket"]
    Z --> AA{"keep-alive?"}
    AA -- "yes" --> L
    AA -- "no" --> AB["close connection"]
end
```

## Lecture du schéma

- `main.cpp` charge la configuration, initialise `WebServer`, puis lance la boucle principale.
- `EventLoop` gère l'I/O non bloquant avec `epoll`, les sockets d'écoute et les connexions clients.
- `Connection` stocke les buffers d'entrée/sortie et l'état de vie de la socket.
- `HttpRequestParser` transforme les octets reçus en `HttpRequest` après validation du message.
- `Router` choisit le `ServerConfig` et le `LocationConfig` adaptés, puis construit un `RequestContext`.
- `StaticHandler` produit la réponse finale selon la méthode HTTP et le contexte de route.
- `HttpResponse` sérialise la réponse HTTP à envoyer au client.

## Notes

- Le serveur est prévu pour fonctionner en boucle avec `keep-alive` tant que la requête et la connexion le permettent.
- Les erreurs HTTP peuvent être remplacées par des pages personnalisées si elles sont déclarées dans la configuration.
- Dans l'état actuel du code, les sockets d'écoute sont initialisés par `EventLoop` sur les ports `8080` et `8081`.
