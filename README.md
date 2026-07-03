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

    subgraph IO["Network layer"]
        H --> I["ListeningSocket"]
        I --> J["accept client"]
        J --> K["Connection"]
        K --> L["read socket"]
        K --> M["write socket"]
    end

    subgraph HTTP["HTTP parsing"]
        L --> N["HttpRequestParser"]
        N --> O{"complete and valid?"}
        O -- "no" --> P["HTTP 400"]
        O -- "yes" --> Q["Router"]
    end

    subgraph ROUTING["Route resolution"]
        Q --> R["ServerConfig"]
        Q --> S["LocationConfig"]
        R --> T["RequestContext"]
        S --> T
        T --> U["root / index / autoindex"]
        T --> V["redirect / allow / cgi / errors / upload"]
    end

    subgraph HANDLER["Request handling"]
        T --> W["StaticHandler"]
        W --> X{"GET / HEAD / POST / DELETE"}
        X --> Y["HttpResponse"]
        P --> Y
    end

    Y --> Z["serialize response"]
    Z --> M
    M --> AA{"keep-alive?"}
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
