import { Outlet, NavLink, useNavigate } from 'react-router-dom'
import { LayoutDashboard, Users, LogOut, Plus } from 'lucide-react'

function Layout({ user, onLogout }) {
  const navigate = useNavigate()

  const handleLogout = () => {
    onLogout()
    navigate('/login')
  }

  return (
    <div style={{ display: 'flex', minHeight: '100vh' }}>
      <aside style={{
        width: '250px',
        backgroundColor: '#1e1b4b',
        color: 'white',
        padding: '24px 0',
        display: 'flex',
        flexDirection: 'column',
      }}>
        <div style={{ padding: '0 24px', marginBottom: '32px' }}>
          <h1 style={{ fontSize: '20px', fontWeight: '700' }}>Lead Manager</h1>
          <p style={{ fontSize: '13px', color: '#a5b4fc', marginTop: '4px' }}>{user.name}</p>
        </div>

        <nav style={{ flex: 1 }}>
          <NavLink to="/" end style={({ isActive }) => navStyle(isActive)}>
            <LayoutDashboard size={18} /> Dashboard
          </NavLink>
          <NavLink to="/leads" style={({ isActive }) => navStyle(isActive)}>
            <Users size={18} /> Leads
          </NavLink>
          <NavLink to="/leads/new" style={({ isActive }) => navStyle(isActive)}>
            <Plus size={18} /> New Lead
          </NavLink>
        </nav>

        <button onClick={handleLogout} style={{
          display: 'flex', alignItems: 'center', gap: '10px',
          padding: '12px 24px', color: '#e0e7ff', background: 'none',
          fontSize: '14px', width: '100%', textAlign: 'left',
        }}>
          <LogOut size={18} /> Logout
        </button>
      </aside>

      <main style={{ flex: 1, padding: '32px', overflow: 'auto' }}>
        <Outlet />
      </main>
    </div>
  )
}

function navStyle(isActive) {
  return {
    display: 'flex',
    alignItems: 'center',
    gap: '10px',
    padding: '12px 24px',
    color: isActive ? '#fff' : '#c7d2fe',
    backgroundColor: isActive ? '#4338ca' : 'transparent',
    fontSize: '14px',
    fontWeight: isActive ? '600' : '400',
    transition: 'all 0.2s',
  }
}

export default Layout
