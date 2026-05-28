# Webserv 42 - Analyse et todolist

## Etat actuel du depot

Le depot contient une base minimale de serveur HTTP:

- Makefile qui compile `src/**/*.cpp` en `webserv` avec `-Wall -Wextra -Werror -std=c++98`.
- `main.cpp` impose un argument de configuration, mais la configuration est ignoree.
- `Config`, `ServerConfig`, `LocationConfig` existent surtout comme structures de donnees, sans parsing.
- `EventLoop` utilise `epoll` et cree deux ports en dur: `8080` et `8081`.
- `ListeningSocket` ouvre des sockets TCP, bind/listen, et les passe en non-bloquant.
- `Connection` gere un fd client avec buffers `in_buffer` et `out_buffer`.
- `HttpRequestParser` parse une requete HTTP tres simple.
- `HttpResponse` serialise une reponse basique.
- `StaticHandler` sert des fichiers statiques depuis `./`.
- `Router` est declare mais non implemente/utilise.
- `server.cpp` et `client.cpp` sont des essais UDP hors build.
- `README.md`, fichiers de config de demo, error pages, tests et CGI sont absents.

Compilation: `make re` passe hors sandbox avec `-Wall -Wextra -Werror -std=c++98`, puis `make` ne relink pas inutilement. Le demarrage n'a pas ete valide dans ce sandbox: l'ouverture reseau semble bloquee ici et le serveur echoue sur le port `8080`.

Legende:

- `[x]` fait.
- `[~]` partiel: une base existe dans le code, mais le libelle complet n'est pas encore respecte.
- `[ ]` non fait ou non prouve.

## Ecarts majeurs avec le sujet

- La configuration n'est pas parse et n'influence pas le runtime.
- Les ports, hosts et roots sont hardcodes.
- Le serveur ne respecte pas encore toutes les contraintes I/O du sujet:
  - `epoll_create1` est utilise alors que le sujet liste `epoll_create`.
  - `recv`/`send` consultent `errno` apres l'appel pour gerer `EAGAIN`/`EWOULDBLOCK`, ce que le sujet interdit pour read/write.
  - Les boucles `recv`/`send` jusqu'a `EAGAIN` risquent de monopoliser l'event loop.
- Les methodes obligatoires ne sont pas completes:
  - GET statique minimal seulement.
  - POST non implemente.
  - DELETE non implemente.
  - Upload non implemente.
- Pas de routing par `server`/`location`.
- Pas de `client_max_body_size`.
- Pas de pages d'erreur par defaut ni pages configurees.
- Pas de redirection.
- Pas d'autoindex.
- Pas de CGI.
- Pas de chunked request decoding.
- Pas de timeouts, donc une requete peut rester incomplete indefiniment.
- Pas de tests de regression/stress.
- README obligatoire absent.

## Epic 1 - Fondations build, hygiene et structure

Objectif: rendre la base propre, portable dans le cadre 42, et facile a etendre.

- [x] Verifier que le Makefile respecte strictement le sujet: `NAME`, `all`, `clean`, `fclean`, `re`, pas de relink inutile.
- [ ] Remplacer `find -printf` si la correction vise aussi macOS, car `-printf` n'est pas portable BSD.
- [ ] Decider si `-Ofast` reste dans `CFLAGS`; preferer un build de correction simple et debuggable.
- [ ] Retirer ou deplacer `server.cpp` et `client.cpp` hors des fichiers soumis si ce sont seulement des notes/experiences.
- [ ] Initialiser toutes les donnees de config avec des constructeurs explicites:
  - `ServerConfig::port`
  - `ServerConfig::client_max_body_size`
  - `LocationConfig::autoindex`
- [ ] Rendre les signatures const-correct:
  - `Config(const std::string& filename)`
  - accesseurs quand necessaire.
- [~] Centraliser les helpers communs:
  - non-blocking fd
  - close fd safe
  - status reason phrases
  - string trim/lowercase
  - path utilities.
- [~] Fermer proprement `epfd` et tous les sockets a l'arret.
- [~] Supprimer les logs de debug bruyants avant evaluation ou les garder derriere `DEBUG`.

Validation:

- [x] `make re` passe avec `-Wall -Wextra -Werror -std=c++98`.
- [x] `make` relance immediatement sans relink inutile.
- [ ] Aucun fichier experimental hors build ne peut perturber l'evaluation.

## Epic 2 - Parser de configuration

Objectif: lire un fichier inspire de NGINX et remplir `Config`.

- [ ] Definir une grammaire simple et documentee:
  - `server { ... }`
  - `location /path { ... }`
  - directives terminees par `;`
  - commentaires `#`
  - blocs imbriques limites a `server/location`.
- [ ] Implementer un tokenizer C++98:
  - mots
  - `{`
  - `}`
  - `;`
  - detection EOF propre.
- [ ] Implementer le parser `Config`:
  - refuser les directives inconnues avec message clair.
  - refuser accolades mal fermees.
  - refuser directives dupliquees si ambigu.
  - appliquer des valeurs par defaut.
- [ ] Supporter directives server obligatoires/utiles:
  - `listen host:port;`
  - `listen port;`
  - `server_name name;` optionnel.
  - `root path;`
  - `index file;`
  - `error_page code path;`
  - `client_max_body_size size;`
- [ ] Supporter directives location:
  - `methods GET POST DELETE;`
  - `root path;`
  - `index file;`
  - `autoindex on|off;`
  - `return code url;` pour redirection.
  - `upload_dir path;`
  - `cgi .ext interpreter_path;`
- [ ] Valider les valeurs:
  - port entre 1 et 65535.
  - taille body avec suffixes optionnels `K`, `M`.
  - status error page numerique.
  - location path commence par `/`.
- [ ] Preparer des fichiers de config de demo:
  - config minimale.
  - multi ports.
  - locations statiques.
  - upload.
  - CGI.
  - error pages.

Validation:

- [ ] Une config invalide fait echouer `./webserv config` avec code retour non-zero.
- [ ] Une config valide produit toutes les `ServerConfig` attendues.
- [ ] Un mode test parser ou tests unitaires couvrent erreurs de syntaxe et cas nominal.

## Epic 3 - Sockets, event loop et contraintes non-bloquantes

Objectif: respecter strictement le modele I/O du sujet.

- [ ] Remplacer les listeners hardcodes de `EventLoop` par les `listen` issus de `Config`.
- [ ] Gerer plusieurs `server` sur un meme port sans double bind inutile.
- [ ] Associer chaque fd listener a sa config server ou a un groupe de servers.
- [ ] Remplacer `epoll_create1(0)` par `epoll_create(size)` pour rester dans les fonctions autorisees.
- [x] Garder un seul appel `epoll_wait` central pour toutes les I/O sockets.
- [x] Sur sockets clients, ne faire `recv` que quand `EPOLLIN` est signale.
- [x] Sur sockets clients, ne faire `send` que quand `EPOLLOUT` est signale.
- [ ] Retirer la logique qui consulte `errno` apres `recv`/`send`.
- [ ] Eviter les boucles read/write jusqu'a `EAGAIN`; traiter une quantite bornee par event.
- [x] Gerer les envois partiels:
  - garder le reste dans `out_buffer`.
  - laisser `EPOLLOUT` actif tant que le buffer n'est pas vide.
- [x] Gerer les lectures partielles:
  - accumuler dans `in_buffer`.
  - parser seulement quand la requete est complete.
- [ ] Ajouter timeouts de connexion:
  - headers incomplets.
  - body incomplet.
  - idle keep-alive.
- [~] Gerer `EPOLLHUP`, `EPOLLERR`, fermeture client, et cleanup sans fuite.
- [x] Eviter que la suppression d'une connexion invalide l'iteration en cours.

Validation:

- [ ] Plusieurs clients simultanes peuvent charger des pages.
- [ ] Un client lent ne bloque pas les autres.
- [ ] Un client qui deconnecte pendant upload/download ne crash pas.
- [ ] Stress test basique sans indisponibilite.

## Epic 4 - Parser HTTP robuste

Objectif: transformer un flux TCP en requetes HTTP valides et exploitables.

- [~] Separer clairement:
  - detection de requete complete.
  - parsing de start-line.
  - parsing headers.
  - parsing body.
- [~] Accepter au minimum `HTTP/1.0` et `HTTP/1.1` si choisi, ou documenter strictement le choix.
- [~] Valider methodes:
  - reconnues: GET, POST, DELETE.
  - non autorisees par location: 405.
  - inconnues: 400 ou 501 selon strategie.
- [ ] Parser headers de facon case-insensitive.
- [~] Nettoyer correctement `\r\n`.
- [~] Gerer `Content-Length`:
  - absent.
  - invalide.
  - negatif/impossible.
  - multiple.
- [ ] Gerer `Transfer-Encoding: chunked`:
  - unchunk avant handlers/CGI.
  - detecter chunk invalide.
- [~] Extraire URI:
  - path.
  - query string.
  - percent-decoding si necessaire pour fichiers.
- [~] Gerer Host pour HTTP/1.1:
  - obligatoire.
  - utile pour virtual hosts si implemente.
- [~] Gerer keep-alive:
  - HTTP/1.1 keep-alive par defaut sauf `Connection: close`.
  - HTTP/1.0 close par defaut sauf `Connection: keep-alive`.
- [~] Supporter plusieurs requetes dans `in_buffer` apres une lecture, ou decider de fermer apres une reponse.
- [ ] Appliquer limites:
  - taille headers max.
  - taille body selon config.

Validation:

- [ ] Requetes partielles fonctionnent.
- [ ] Requetes pipelined ne cassent pas le serveur.
- [ ] Requetes invalides retournent les bons statuts sans crash.
- [ ] Upload chunked arrive dechunked au handler/CGI.

## Epic 5 - Routing server/location

Objectif: choisir la bonne configuration effective pour chaque requete.

- [ ] Implementer `Router`.
- [ ] Selectionner le `ServerConfig` depuis:
  - fd listener ou port local.
  - header `Host` si virtual host implemente.
  - premier server du couple host:port comme default server.
- [ ] Matcher la meilleure `LocationConfig` par prefixe le plus long.
- [ ] Fusionner config server + location:
  - root.
  - index.
  - methods.
  - autoindex.
  - upload_dir.
  - cgi map.
  - redirection.
  - error_pages.
  - client_max_body_size.
- [ ] Normaliser les paths pour eviter les traversals.
- [ ] Preparer un objet `RequestContext` donne aux handlers.

Validation:

- [ ] Deux ports peuvent servir deux roots differents.
- [ ] Deux locations d'un meme server appliquent des roots/methods differents.
- [ ] Le prefixe le plus long gagne.

## Epic 6 - Reponses HTTP et gestion d'erreurs

Objectif: produire des reponses correctes, completes et coherentes.

- [~] Centraliser les reason phrases:
  - 200 OK
  - 201 Created
  - 204 No Content
  - 301/302 redirect
  - 400 Bad Request
  - 403 Forbidden
  - 404 Not Found
  - 405 Method Not Allowed
  - 413 Payload Too Large
  - 500 Internal Server Error
  - 501 Not Implemented
  - 504 Gateway Timeout si CGI timeout.
- [ ] Ne pas encoder `Connection` via un faux header `keep-alive`.
- [~] Respecter les headers fournis par les handlers.
- [ ] Ajouter `Date` si souhaite, sinon garder minimal.
- [ ] Ajouter `Allow` sur 405.
- [ ] Ajouter `Location` sur redirect.
- [ ] Implementer default error pages HTML si aucune page configuree.
- [ ] Servir les `error_page` configurees si disponibles.
- [~] Eviter de recalculer un Content-Length faux:
  - body vide pour 204.
  - body absent pour HEAD si HEAD supporte.
- [~] Gerer MIME types de facon plus complete.

Validation:

- [ ] Comparer headers/status avec NGINX sur cas simples.
- [ ] Toutes les erreurs obligatoires ont une page par defaut.
- [ ] Les redirects fonctionnent dans navigateur et curl.

## Epic 7 - Static files, index et autoindex

Objectif: servir un site statique complet.

- [ ] Remplacer root hardcode `./` par root effectif du routing.
- [~] Utiliser `stat` pour distinguer:
  - fichier.
  - dossier.
  - absent.
  - permissions insuffisantes.
- [~] Si ressource dossier:
  - si path sans `/`, optionnellement redirect vers `/`.
  - chercher index configure.
  - si pas d'index et autoindex on, generer listing.
  - si pas d'index et autoindex off, retourner 403.
- [~] Proteger contre path traversal:
  - normaliser path.
  - verifier que le chemin final reste sous root.
- [x] Supporter GET.
- [~] Decider si HEAD est supporte; si oui, l'ajouter proprement au parser/methods.
- [x] Lire fichiers regulierement sans poll, autorise par le sujet.
- [x] Gerer fichiers binaires sans corruption.

Validation:

- [ ] HTML/CSS/images fonctionnent dans navigateur.
- [ ] `/dir/` sert l'index ou l'autoindex selon config.
- [ ] `../` et encodages equivalents ne sortent pas du root.

## Epic 8 - Methodes POST, upload et DELETE

Objectif: couvrir les methodes obligatoires du sujet.

- [ ] POST:
  - si location upload active, ecrire le body dans `upload_dir`.
  - choisir une strategie de nommage de fichier.
  - retourner 201 Created ou status coherent.
  - verifier permissions et existence du dossier.
- [ ] POST sans upload ni CGI:
  - retourner 405/404/501 selon configuration retenue.
- [ ] DELETE:
  - mapper URI vers fichier sous root.
  - refuser dossiers sauf choix explicite.
  - supprimer via fonction autorisee si disponible dans sujet; verifier si `unlink` est autorise avant usage.
  - si `unlink` non autorise dans votre version, revoir strategie avec evaluateurs/sujet local.
  - retourner 204 si suppression OK.
  - retourner 404 si absent, 403 si interdit.
- [ ] Appliquer `methods` par location avant handler.
- [ ] Appliquer `client_max_body_size` avant d'accumuler un body trop gros.

Validation:

- [ ] Upload avec curl cree un fichier attendu.
- [ ] Body trop gros retourne 413 sans tuer le serveur.
- [ ] DELETE supprime uniquement sous le root autorise.

## Epic 9 - CGI

Objectif: executer au moins un CGI conforme au sujet.

- [ ] Choisir un CGI minimum a supporter:
  - Python recommande pour demo simple.
  - PHP-CGI possible si disponible.
- [ ] Detecter CGI par extension configuree:
  - exemple `.py /usr/bin/python3`.
- [ ] Construire l'environnement CGI:
  - REQUEST_METHOD
  - SCRIPT_NAME
  - SCRIPT_FILENAME
  - PATH_INFO
  - QUERY_STRING
  - CONTENT_LENGTH
  - CONTENT_TYPE
  - SERVER_PROTOCOL
  - GATEWAY_INTERFACE
  - REDIRECT_STATUS si necessaire.
- [ ] Fournir body au CGI via pipe stdin.
- [ ] Lire stdout CGI via pipe non-bloquant gere par l'event loop, ou modele controle sans bloquer.
- [ ] Utiliser `fork`, `execve`, `pipe`, `dup2`, `waitpid` uniquement pour CGI.
- [ ] Changer le working directory du CGI correctement si besoin.
- [ ] Parser la sortie CGI:
  - headers CGI.
  - ligne vide.
  - body.
  - absence de Content-Length: EOF marque fin.
- [ ] Gerer timeout CGI et tuer process si necessaire.
- [ ] Gerer chunked request: le CGI recoit le body dechunked.

Validation:

- [ ] CGI GET recoit query string.
- [ ] CGI POST recoit body.
- [ ] CGI lent timeout sans bloquer le serveur.
- [ ] CGI invalide retourne 500/502 selon strategie.

## Epic 10 - Resilience, securite et limites

Objectif: le serveur reste operationnel quoi qu'envoie le client.

- [ ] Ajouter limites de taille:
  - request line.
  - headers.
  - body.
  - output buffer.
- [ ] Ajouter timeouts:
  - header timeout.
  - body timeout.
  - keep-alive idle.
  - CGI timeout.
- [ ] Gerer erreurs d'allocation autant que possible sans crash.
- [ ] Ne jamais dereferencer une config absente.
- [ ] Ne jamais faire confiance au path client.
- [ ] Traiter signaux utiles:
  - SIGINT pour arret propre.
  - SIGPIPE ignore ou gere pour eviter crash sur send.
- [ ] Verifier que le serveur continue apres:
  - requete malformee.
  - body annonce mais jamais envoye.
  - client ferme pendant send.
  - CGI crash.

Validation:

- [ ] Fuzz manuel avec netcat/telnet.
- [ ] Tests Python avec sockets bruts.
- [ ] Stress avec plusieurs clients paralleles.

## Epic 11 - Tests et comparaison NGINX

Objectif: prouver chaque exigence avant evaluation.

- [ ] Ajouter un dossier `tests/` ou scripts locaux.
- [ ] Tests parser config.
- [ ] Tests parser HTTP.
- [ ] Tests integration serveur:
  - GET fichier.
  - GET 404.
  - directory index.
  - autoindex.
  - methods 405.
  - POST upload.
  - DELETE.
  - body trop gros.
  - redirect.
  - CGI GET/POST.
  - chunked POST.
  - keep-alive.
- [ ] Tests multi ports:
  - port A root A.
  - port B root B.
- [ ] Tests concurrence:
  - 50+ connexions.
  - clients lents.
  - upload/download simultanes.
- [ ] Comparer quelques cas avec NGINX:
  - status.
  - headers importants.
  - comportement dossier/index.

Validation:

- [ ] Une commande documentee lance les tests.
- [ ] Les tests peuvent etre relances apres chaque refactor.
- [ ] Les cas de demonstration evaluation sont prets.

## Epic 12 - README et assets d'evaluation

Objectif: satisfaire les exigences administratives et faciliter la soutenance.

- [ ] Creer `README.md` en anglais.
- [ ] Premiere ligne exactement au format demande:
  - italicized.
  - `This project has been created as part of the 42 curriculum by <login...>.`
- [ ] Section `Description`.
- [ ] Section `Instructions`:
  - compilation.
  - execution.
  - exemple de config.
- [ ] Section `Resources`:
  - RFC HTTP.
  - man pages.
  - documentation CGI.
  - explication de l'usage de l'IA.
- [ ] Ajouter configs de demo.
- [ ] Ajouter site statique de demo.
- [ ] Ajouter pages d'erreur.
- [ ] Ajouter script CGI de demo.
- [ ] Ajouter dossier upload vide avec `.gitkeep` si besoin.

Validation:

- [ ] Un peer peut cloner, compiler, lancer, tester sans aide orale.
- [ ] Toutes les features obligatoires ont un exemple pret.

## Ordre recommande d'implementation

1. Nettoyer build/structure et initialiser les configs.
2. Implementer parser de configuration.
3. Brancher event loop sur config et corriger contraintes epoll/read/write.
4. Refactor HTTP parser + response/status.
5. Implementer router server/location.
6. Finaliser static files, index, autoindex et erreurs.
7. Implementer POST upload et DELETE.
8. Implementer CGI.
9. Ajouter timeouts, limites et resilience.
10. Ajouter tests/stress et comparer avec NGINX.
11. Rediger README et preparer assets d'evaluation.

## Definition of done minimale pour le mandatory

- [ ] `./webserv [config]` fonctionne avec un fichier de config reel.
- [ ] Le serveur est non-bloquant et pilote toutes les I/O sockets via un seul `epoll_wait`.
- [ ] GET/POST/DELETE fonctionnent selon les locations.
- [ ] Site statique complet servi dans un navigateur.
- [ ] Upload client operationnel.
- [ ] Plusieurs ports servent des contenus differents.
- [ ] Error pages par defaut et configurees.
- [ ] client body max applique.
- [ ] Autoindex, index, redirection et methods par route.
- [ ] CGI minimum fonctionnel.
- [ ] Pas de crash sur requetes invalides ou clients deconnectes.
- [ ] Tests et fichiers de demo prets pour evaluation.
