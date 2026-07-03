# webserv

Architecture globale du projet `webserv` et flux principal d'une requête HTTP.

```mermaid
flowchart TD
    A[main.cpp] --> B[Lecture de la config]
    B --> C[Config]
    C --> D[Config::validate()]
    D --> E[WebServer::getInstance()]
    E --> F[WebServer::appliConfig()]
    F --> G[WebServer::run()]
    G --> H[EventLoop::run()]

    subgraph IO["Couche réseau"]
        H --> I[ListeningSocket]
        I --> J[accept() client]
        J --> K[Connection]
        K --> L[readFromSocket()]
        K --> M[writeToSocket()]
    end

    subgraph HTTP["Parsing HTTP"]
        L --> N[HttpRequestParser::parse()]
        N --> O{Requête complète et valide ?}
        O -- non --> P[HttpResponse 400]
        O -- oui --> Q[Router::resolve()]
    end

    subgraph ROUTING["Résolution de route"]
        Q --> R[ServerConfig]
        Q --> S[LocationConfig]
        R --> T[RequestContext]
        S --> T
        T --> U[root / index / autoindex]
        T --> V[redirect / allow / cgi / error_pages / upload_dir]
    end

    subgraph HANDLER["Traitement métier"]
        T --> W[StaticHandler]
        W --> X{GET / HEAD / POST / DELETE}
        X --> Y[HttpResponse]
        P --> Y
    end

    Y --> Z[HttpResponse::toString()]
    Z --> M
    M --> AA{keep-alive ?}
    AA -- oui --> L
    AA -- non --> AB[Fermeture de la connexion]
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
