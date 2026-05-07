# Lead Manager C++

Aplicacao full stack em C++ para gestao de leads com:

- Backend REST em C++
- Autenticacao JWT
- Persistencia no Firebase Firestore via service account
- Frontend desktop-first em HTML, CSS e JavaScript
- Importacao CSV e Excel
- Exportacao CSV e Excel

## Estrutura

- `src/`: backend C++
- `include/`: cabecalhos da aplicacao
- `public/`: frontend e assets
- `config.json`: configuracao local
- `external/`: dependencias header-only

## Colecoes Firestore

Por causa do requisito de isolamento por nome do modelo e linguagem, o sistema usa por padrao:

- `gpt_5_cpp_users`
- `gpt_5_cpp_leads`

Se quiser mudar o prefixo, altere `collection_prefix` em `config.json`.

## Requisitos

- Windows
- CMake 3.20+
- Clang ou GCC com suporte a C++20
- Firebase/Firestore habilitado no projeto da service account

## Build

No PowerShell, dentro da pasta do projeto:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Execucao

```powershell
cd build
.\lead_manager.exe
```

Depois abra:

- [http://localhost:8080](http://localhost:8080)

## Configuracao

O arquivo `config.json` ja vem apontando para a service account informada:

- `firebase_service_account_path`: caminho do JSON do Firebase
- `jwt_secret`: segredo usado para assinar o JWT da aplicacao
- `collection_prefix`: prefixo das colecoes criadas no Firestore
- `port`: porta HTTP local

## Funcionalidades

- Cadastro e login de usuarios com senha criptografada via PBKDF2-HMAC-SHA256
- JWT proprio da aplicacao para proteger as rotas REST
- CRUD completo de leads
- Filtros por status, fonte, periodo e texto livre
- Paginacao e ordenacao
- Dashboard com resumo por status e capturas por data
- Tela de detalhes do lead com historico de interacoes
- Importacao em lote por CSV
- Importacao em lote por Excel no frontend usando SheetJS
- Exportacao CSV no backend
- Exportacao Excel no frontend

## Observacoes

- O servidor sobe em HTTP local na porta `8080`. Para producao, o ideal e publicar atras de HTTPS.
- O acesso ao Firestore e feito com OAuth 2.0 de service account.
- O frontend e servido pela propria aplicacao C++.
