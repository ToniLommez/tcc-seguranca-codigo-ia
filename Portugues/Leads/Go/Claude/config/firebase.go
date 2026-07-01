package config

// ============================================================================
// NOTA (execução offline para captura de tela do TCC):
// Esta versão substitui o Firestore por um armazenamento EM MEMÓRIA, permitindo
// rodar o sistema sem credenciais do Firebase. Dados de exemplo são semeados na
// inicialização. NÃO use em produção.
// ============================================================================

import (
	"sync"
	"time"

	"lead-manager/models"
)

// DemoUserID é o dono dos dados de exemplo (usuário de demonstração).
const DemoUserID = "demo-user"

// Store é um armazenamento simples em memória, protegido por mutex.
type Store struct {
	mu    sync.RWMutex
	users map[string]models.User
	leads map[string]models.Lead
}

// DB é a instância global do armazenamento.
var DB = &Store{
	users: map[string]models.User{},
	leads: map[string]models.Lead{},
}

// InitFirebase mantém o nome por compatibilidade com main.go, mas agora apenas
// inicializa o armazenamento em memória e semeia os dados de exemplo.
func InitFirebase() error {
	DB.seed()
	return nil
}

// CloseFirebase mantido por compatibilidade (no-op).
func CloseFirebase() {}

// ── Usuários ────────────────────────────────────────────────────────────────
func (s *Store) AddUser(u models.User) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.users[u.ID] = u
}

func (s *Store) UserByID(id string) (models.User, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	u, ok := s.users[id]
	return u, ok
}

func (s *Store) UserByEmail(email string) (models.User, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	for _, u := range s.users {
		if u.Email == email {
			return u, true
		}
	}
	return models.User{}, false
}

// ── Leads ─────────────────────────────────────────────────────────────────---
func (s *Store) AddLead(l models.Lead) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.leads[l.ID] = l
}

func (s *Store) UpdateLead(l models.Lead) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.leads[l.ID] = l
}

func (s *Store) DeleteLead(id string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.leads, id)
}

func (s *Store) LeadByID(id string) (models.Lead, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	l, ok := s.leads[id]
	return l, ok
}

func (s *Store) LeadsByUser(userID string) []models.Lead {
	s.mu.RLock()
	defer s.mu.RUnlock()
	out := []models.Lead{}
	for _, l := range s.leads {
		if l.UserID == userID {
			out = append(out, l)
		}
	}
	return out
}

// seed popula o armazenamento com um usuário e leads de exemplo.
func (s *Store) seed() {
	s.AddUser(models.User{
		ID:        DemoUserID,
		Name:      "Lommez Tecnologia",
		Email:     "contato@lommeztech.com.br",
		CreatedAt: time.Now().AddDate(0, 0, -40),
	})

	d := func(n int) time.Time { return time.Now().AddDate(0, 0, -n) }

	samples := []models.Lead{
		{FullName: "Ana Beatriz Lima", Email: "ana.lima@techsolutions.com.br", Phone: "(31) 98812-4471", Company: "Tech Solutions", Role: "CTO", Source: models.SourceIndicacao, Status: models.StatusQualificado, Notes: "Interessada no plano Enterprise.", CapturedAt: d(2)},
		{FullName: "Carlos Eduardo Souza", Email: "carlos.souza@nuvemsistemas.com", Phone: "(11) 99654-3120", Company: "Nuvem Sistemas", Role: "Gerente de TI", Source: models.SourceSite, Status: models.StatusNovo, CapturedAt: d(1)},
		{FullName: "Mariana Ferreira", Email: "mariana.f@datacorp.io", Phone: "(21) 98123-7788", Company: "DataCorp", Role: "Head de Dados", Source: models.SourceLinkedIn, Status: models.StatusEmContato, Notes: "Reunião agendada para sexta.", CapturedAt: d(5)},
		{FullName: "João Pedro Alves", Email: "joao.alves@fintechbr.com", Phone: "(41) 99711-2034", Company: "FinTech BR", Role: "CEO", Source: models.SourceEvento, Status: models.StatusQualificado, CapturedAt: d(10)},
		{FullName: "Fernanda Costa", Email: "fernanda.costa@logisoft.com.br", Phone: "(51) 98456-9001", Company: "LogiSoft", Role: "Diretora de Operações", Source: models.SourceIndicacao, Status: models.StatusNovo, CapturedAt: d(0)},
		{FullName: "Rafael Oliveira", Email: "rafael@cloudware.dev", Phone: "(31) 99888-1212", Company: "CloudWare", Role: "Arquiteto de Software", Source: models.SourceSite, Status: models.StatusPerdido, Notes: "Optou por concorrente.", CapturedAt: d(15)},
		{FullName: "Juliana Martins", Email: "juliana.martins@agrotech.com", Phone: "(62) 98700-3344", Company: "AgroTech", Role: "Gerente Comercial", Source: models.SourceEmail, Status: models.StatusEmContato, CapturedAt: d(3)},
		{FullName: "Bruno Henrique", Email: "bruno.h@devhouse.com.br", Phone: "(11) 99123-8890", Company: "DevHouse", Role: "Tech Lead", Source: models.SourceLinkedIn, Status: models.StatusQualificado, CapturedAt: d(7)},
		{FullName: "Patrícia Gomes", Email: "patricia.gomes@smartdata.ai", Phone: "(81) 98555-2210", Company: "SmartData", Role: "Product Manager", Source: models.SourceEvento, Status: models.StatusNovo, CapturedAt: d(1)},
		{FullName: "Lucas Pereira", Email: "lucas.pereira@bytelabs.com", Phone: "(47) 99432-7766", Company: "ByteLabs", Role: "Engenheiro de Dados", Source: models.SourceSite, Status: models.StatusEmContato, CapturedAt: d(4)},
		{FullName: "Camila Rocha", Email: "camila.rocha@netcore.com.br", Phone: "(31) 98321-4567", Company: "NetCore", Role: "VP de Tecnologia", Source: models.SourceIndicacao, Status: models.StatusQualificado, Notes: "Fechamento previsto para o mês.", CapturedAt: d(9)},
		{FullName: "Thiago Mendes", Email: "thiago.mendes@inovati.com", Phone: "(85) 99777-0033", Company: "InovaTI", Role: "Coordenador", Source: models.SourceOutro, Status: models.StatusPerdido, CapturedAt: d(20)},
		{FullName: "Larissa Dias", Email: "larissa.dias@webforge.dev", Phone: "(11) 98444-9988", Company: "WebForge", Role: "Designer de Produto", Source: models.SourceLinkedIn, Status: models.StatusNovo, CapturedAt: d(2)},
		{FullName: "Gabriel Santos", Email: "gabriel.santos@codefactory.com.br", Phone: "(21) 99001-5566", Company: "CodeFactory", Role: "Gerente de Projetos", Source: models.SourceSite, Status: models.StatusEmContato, CapturedAt: d(6)},
	}

	for i, l := range samples {
		l.ID = "lead-" + itoa(i+1)
		l.UserID = DemoUserID
		l.CreatedAt = l.CapturedAt
		l.UpdatedAt = l.CapturedAt
		if l.Interactions == nil {
			l.Interactions = []models.Interaction{}
		}
		s.AddLead(l)
	}
}

// itoa minimalista para evitar dependência de strconv no seed.
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var b [20]byte
	pos := len(b)
	for n > 0 {
		pos--
		b[pos] = byte('0' + n%10)
		n /= 10
	}
	return string(b[pos:])
}
