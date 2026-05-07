import { useState, useEffect } from 'react'
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, PieChart, Pie, Cell } from 'recharts'
import { Users, UserPlus, UserCheck, UserX } from 'lucide-react'
import { leadsAPI } from '../services/api'

const STATUS_COLORS = { new: '#6366f1', contacted: '#f59e0b', qualified: '#10b981', lost: '#ef4444' }

function Dashboard() {
  const [stats, setStats] = useState(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    leadsAPI.stats().then((res) => {
      setStats(res.data)
      setLoading(false)
    }).catch(() => setLoading(false))
  }, [])

  if (loading) return <p>Loading dashboard...</p>
  if (!stats) return <p>Failed to load statistics.</p>

  const statusData = Object.entries(stats.status_counts).map(([name, value]) => ({ name, value }))
  const monthlyData = Object.entries(stats.monthly_captures).map(([month, count]) => ({ month, count }))
  const sourceData = Object.entries(stats.source_counts).map(([name, value]) => ({ name, value }))

  const cards = [
    { label: 'Total Leads', value: stats.total_leads, icon: Users, color: '#6366f1' },
    { label: 'New', value: stats.status_counts.new, icon: UserPlus, color: '#6366f1' },
    { label: 'Qualified', value: stats.status_counts.qualified, icon: UserCheck, color: '#10b981' },
    { label: 'Lost', value: stats.status_counts.lost, icon: UserX, color: '#ef4444' },
  ]

  return (
    <div>
      <h2 style={{ fontSize: '24px', fontWeight: '700', marginBottom: '24px' }}>Dashboard</h2>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '20px', marginBottom: '32px' }}>
        {cards.map((card) => (
          <div key={card.label} style={styles.card}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <div>
                <p style={{ fontSize: '13px', color: '#6b7280' }}>{card.label}</p>
                <p style={{ fontSize: '28px', fontWeight: '700', color: card.color }}>{card.value}</p>
              </div>
              <card.icon size={32} color={card.color} strokeWidth={1.5} />
            </div>
          </div>
        ))}
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '24px', marginBottom: '32px' }}>
        <div style={styles.chartCard}>
          <h3 style={styles.chartTitle}>Leads Over Time</h3>
          {monthlyData.length > 0 ? (
            <ResponsiveContainer width="100%" height={280}>
              <BarChart data={monthlyData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="month" />
                <YAxis />
                <Tooltip />
                <Bar dataKey="count" fill="#6366f1" radius={[4, 4, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          ) : (
            <p style={{ color: '#9ca3af', padding: '40px', textAlign: 'center' }}>No data yet. Add some leads to see trends.</p>
          )}
        </div>

        <div style={styles.chartCard}>
          <h3 style={styles.chartTitle}>By Status</h3>
          {statusData.some((d) => d.value > 0) ? (
            <ResponsiveContainer width="100%" height={280}>
              <PieChart>
                <Pie data={statusData} dataKey="value" nameKey="name" cx="50%" cy="50%" outerRadius={90} label={({ name, value }) => `${name}: ${value}`}>
                  {statusData.map((entry) => (
                    <Cell key={entry.name} fill={STATUS_COLORS[entry.name] || '#9ca3af'} />
                  ))}
                </Pie>
                <Tooltip />
              </PieChart>
            </ResponsiveContainer>
          ) : (
            <p style={{ color: '#9ca3af', padding: '40px', textAlign: 'center' }}>No data yet.</p>
          )}
        </div>
      </div>

      <div style={styles.chartCard}>
        <h3 style={styles.chartTitle}>By Source</h3>
        {sourceData.length > 0 ? (
          <ResponsiveContainer width="100%" height={250}>
            <BarChart data={sourceData} layout="vertical">
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis type="number" />
              <YAxis dataKey="name" type="category" width={100} />
              <Tooltip />
              <Bar dataKey="value" fill="#8b5cf6" radius={[0, 4, 4, 0]} />
            </BarChart>
          </ResponsiveContainer>
        ) : (
          <p style={{ color: '#9ca3af', padding: '40px', textAlign: 'center' }}>No data yet.</p>
        )}
      </div>
    </div>
  )
}

const styles = {
  card: { background: '#fff', padding: '24px', borderRadius: '10px', boxShadow: '0 1px 4px rgba(0,0,0,0.06)' },
  chartCard: { background: '#fff', padding: '24px', borderRadius: '10px', boxShadow: '0 1px 4px rgba(0,0,0,0.06)' },
  chartTitle: { fontSize: '16px', fontWeight: '600', marginBottom: '16px', color: '#374151' },
}

export default Dashboard
