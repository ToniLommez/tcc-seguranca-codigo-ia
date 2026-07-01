# Identificação e Análise de Vulnerabilidades em Código Gerado por LLMs

> **Uma comparação entre prompts em Português e Inglês**

![Linguagens](https://img.shields.io/badge/linguagens-C%2B%2B%20%C2%B7%20Go%20%C2%B7%20Python-blue)
![Modelos](https://img.shields.io/badge/modelos-Claude%20Sonnet%204.6%20%C2%B7%20GPT--5.4%20Codex-8a2be2)
![Análise](https://img.shields.io/badge/análise-Cppcheck%20%C2%B7%20Gosec%20%C2%B7%20Bandit-informational)
![Classificação](https://img.shields.io/badge/classifica%C3%A7%C3%A3o-CWE%20%2F%20OWASP-critical)

Trabalho de Conclusão de Curso — **Bacharelado em Ciência da Computação, PUC Minas**
Autor: **Marcos Antônio Lommez Cândido Ribeiro** · Orientador: **Prof. Matheus Alcântara Souza**

---

## Sobre o trabalho

Modelos de linguagem de grande porte (LLMs) já convertem instruções em linguagem natural
diretamente em código funcional, mas **não são treinados para avaliar a segurança do que
produzem**. Este estudo investiga uma dimensão ainda não explorada na literatura:

> **O idioma do prompt — português ou inglês — influencia as vulnerabilidades de
> segurança no código gerado por LLMs?**

Para responder, foram geradas **24 implementações independentes** a partir da primeira
resposta de cada modelo (sem refinamento), analisadas por ferramentas de análise estática
e por revisão manual, com as falhas classificadas segundo o **CWE** (*Common Weakness
Enumeration*) e alinhadas ao **OWASP**.

## Desenho experimental (fatorial 2 × 2 × 2 × 3 = 24)

| Fator | Valores |
|-------|---------|
| **Modelos** | GPT-5.4 Codex (OpenAI) · Claude Sonnet 4.6 (Anthropic) |
| **Idiomas** | Português (PT-BR) · Inglês (EN-US) |
| **Cenários** | Gestão de Leads · Streaming de Áudio |
| **Linguagens** | C++ (baixo nível) · Go (intermediário) · Python (alto nível) |
| **Ferramentas** | Cppcheck (C++) · Gosec (Go) · Bandit (Python) + revisão manual |

## Principais achados

- **O idioma pesa pouco no total:** 56 vulnerabilidades (PT-BR) vs 59 (EN-US) — diferença de
  apenas **5,4%**, insuficiente para eleger um idioma como "mais seguro".
- **As falhas sistêmicas são universais**, independentes de idioma, modelo e linguagem:
  - **CWE-798** (credenciais embutidas) — **24/24 projetos (100%)**
  - **CWE-319/CWE-311** (ausência de TLS) — 17/24 projetos
  - **CWE-942** (CORS com origem curinga) — 16/24 projetos
- **O fator dominante é a linguagem de programação:** C++ lidera a média (5,5 falhas/projeto),
  contra 4,5 em Go e 4,4 em Python — menos abstração transfere mais responsabilidades de
  segurança ao modelo.
- **O idioma muda o *tipo* de falha:** prompts em português tendem a produzir implementações
  C++ mais completas (falhas de baixo nível); prompts em inglês ativam padrões de *framework*
  mais sofisticados em Go e Python.
- **Conclusão prática:** a revisão humana é um requisito inegociável ao se adotar geração
  automática de código — nenhum prompt ou ferramenta eliminou as falhas sistêmicas.

## Estrutura do repositório

```
.
├── Portugues/                # Prompts em PT-BR
│   ├── Leads/                #   Cenário: Gestão de Leads
│   │   ├── C++/{Claude,GPT}
│   │   ├── Go/{Claude,GPT}
│   │   └── Python/{Claude,GPT}
│   └── Stream/               #   Cenário: Streaming de Áudio
│       └── ...               #   (mesma divisão por linguagem e modelo)
├── English/                  # Prompts em EN-US (mesma estrutura)
│   ├── Leads/
│   └── Stream/
└── tex/                      # Artigo em LaTeX (fonte, figuras e PDF)
```

Cada uma das 24 pastas de implementação (`.../<Linguagem>/<Modelo>/`) contém:

| Arquivo | Conteúdo |
|---------|----------|
| `prompt.txt` | O prompt exato enviado ao modelo |
| *(código-fonte)* | A implementação gerada, tal como produzida pelo modelo |
| `report.txt` | Saída da análise estática (Cppcheck / Gosec / Bandit), com mapeamento CWE |
| `security_review.txt` | Revisão manual de segurança, com vulnerabilidades por CWE e severidade |

## Artigo

O texto completo está em [`tex/`](tex/) — fonte LaTeX em [`tex/main.tex`](tex/main.tex),
compilável no [Overleaf](https://www.overleaf.com/), acompanhado das figuras e do PDF final.

## ⚠️ Aviso de segurança

Este repositório contém, **por natureza da pesquisa, código propositalmente vulnerável**,
gerado por LLMs sem qualquer correção. **Não utilize nenhuma destas implementações em
produção.** Os segredos e caminhos de credenciais que aparecem no código (por exemplo,
`"change-me-in-production"`) são **exemplos das próprias falhas estudadas (CWE-798)** e não
correspondem a chaves válidas.

## Como citar

```bibtex
@misc{ribeiro2026vulnerabilidades,
  author       = {Marcos Ant{\^o}nio Lommez C{\^a}ndido Ribeiro and Matheus Alc{\^a}ntara Souza},
  title        = {Identifica{\c c}{\~a}o e An{\'a}lise de Vulnerabilidades em C{\'o}digo
                  Gerado por LLMs: Uma Compara{\c c}{\~a}o entre Portugu{\^e}s e Ingl{\^e}s},
  year         = {2026},
  howpublished = {\url{https://github.com/ToniLommez/tcc-seguranca-codigo-ia}},
  note         = {Trabalho de Conclus{\~a}o de Curso, PUC Minas}
}
```

## Licença

Material acadêmico disponibilizado para fins de **pesquisa e reprodutibilidade**. Reuso do
código gerado deve considerar o aviso de segurança acima.
