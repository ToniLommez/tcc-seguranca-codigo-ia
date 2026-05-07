import { useState, useEffect, useRef } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { Search, Download, Upload, Plus, ChevronLeft, ChevronRight } from 'lucide-react'
import { leadsAPI } from '../services/api'

const STATUS_BADGES = {
  new: { bg: '#eef2ff', color: '#4338ca' },
  contacted: { bg: '#fef3c7', color: '#92400e' },
  qualified: { bg: '#d1fae5', color: '#065f46' },
  lost: { bg: '#fee2e2', color: '#991b1b' },
}

function Leads() {
  const [leads, setLeads] = useState([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [totalPages, setTotalPages] = useState(1)
  const [search, setSearch] = useState('')
  const [statusFilter, setStatusFilter] = useState('')
  const [sourceFilter, setSourceFilter] = useState('')
  const [loading, setLoading] = useState(true)
  const fileInputRef = useRef(null)
  const navigate = useNavigate()

  const fetchLeads = () => {
    setLoading(true)
    const params = { page, per_page: 20 }
    if (search) params.search = search
    if (statusFilter) params.status = statusFilter
    if (sourceFilter) params.source = sourceFilter

    leadsAPI.list(params).then((res) => {
      setLeads(res.data.leads)
      setTotal(res.data.total)
      setTotalPages(res.data.total_pages)
      setLoading(false)
    }).catch(() => setLoading(false))
  }

  useEffect(() => { fetchLeads() }, [page, statusFilter, sourceFilter])

  const handleSearch = (e) => {
    e.preventDefault()
    setPage(1)
    fetchLeads()
  }

  const handleExportCSV = async () => {
    const res = await leadsAPI.exportCSV()
    const url = window.URL.createObjectURL(new Blob([res.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'leads_export.csv')
    document.body.appendChild(link)
    link.click()
    link.remove()
  }

  const handleExportExcel = async () => {
    const res = await leadsAPI.exportExcel()
    const url = window.URL.createObjectURL(new Blob([res.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'leads_export.xlsx')
    document.body.appendChild(link)
    link.click()
    link.remove()
  }

  const handleImport = async (e) => {
    const file = e.target.files[0]
    if (!file) return
    try {
      const res = await leadsAPI.importCSV(file)
      alert(`Imported ${res.data.imported} leads successfully.${res.data.errors.length ? ` Errors: ${res.data.errors.length}` : ''}`)
      fetchLeads()
    } catch (err) {
      alert('Import failed: ' + (err.response?.data?.detail || 'Unknown error'))
    }
    e.target.value = ''
  }

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px' }}>
        <h2 style={{ fontSize: '24px', fontWeight: '700' }}>Leads ({total})</h2>
        <div style={{ display: 'flex', gap: '10px' }}>
          <button onClick={() => fileInputRef.current?.click()} style={styles.btnSecondary}>
            <Upload size={16} /> Import
          </button>
          <input ref={fileInputRef} type="file" accept=".csv,.xlsx,.xls" onChange={handleImport} style={{ display: 'none' }} />
          <button onClick={handleExportCSV} style={styles.btnSecondary}><Download size={16} /> CSV</button>
          <button onClick={handleExportExcel} style={styles.btnSecondary}><Download size={16} /> Excel</button>
          <Link to="/leads/new" style={styles.btnPrimary}><Plus size={16} /> New Lead</Link>
        </div>
      </div>

      <div style={{ display: 'flex', gap: '12px', marginBottom: '20px', flexWrap: 'wrap' }}>
        <form onSubmit={handleSearch} style={{ display: 'flex', gap: '8px', flex: 1, minWidth: '250px' }}>
          <div style={{ position: 'relative', flex: 1 }}>
            <Search size={16} style={{ position: 'absolute', left: '12px', top: '12px', color: '#9ca3af' }} />
            <input
              placeholder="Search name, email, company..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              style={{ paddingLeft: '36px' }}
            />
          </div>
          <button type="submit" style={styles.btnPrimary}>Search</button>
        </form>
        <select value={statusFilter} onChange={(e) => { setStatusFilter(e.target.value); setPage(1) }} style={{ width: '150px' }}>
          <option value="">All Status</option>
          <option value="new">New</option>
          <option value="contacted">Contacted</option>
          <option value="qualified">Qualified</option>
          <option value="lost">Lost</option>
        </select>
        <select value={sourceFilter} onChange={(e) => { setSourceFilter(e.target.value); setPage(1) }} style={{ width: '150px' }}>
          <option value="">All Sources</option>
          <option value="referral">Referral</option>
          <option value="website">Website</option>
          <option value="event">Event</option>
          <option value="social_media">Social Media</option>
          <option value="cold_call">Cold Call</option>
          <option value="other">Other</option>
        </select>
      </div>

      <div style={styles.table}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ borderBottom: '2px solid #e5e7eb' }}>
              <th style={styles.th}>Name</th>
              <th style={styles.th}>Email</th>
              <th style={styles.th}>Company</th>
              <th style={styles.th}>Source</th>
              <th style={styles.th}>Status</th>
              <th style={styles.th}>Date</th>
            </tr>
          </thead>
          <tbody>
            {loading ? (
              <tr><td colSpan={6} style={{ padding: '40px', textAlign: 'center', color: '#9ca3af' }}>Loading...</td></tr>
            ) : leads.length === 0 ? (
              <tr><td colSpan={6} style={{ padding: '40px', textAlign: 'center', color: '#9ca3af' }}>No leads found.</td></tr>
            ) : (
              leads.map((lead) => (
                <tr key={lead.id} onClick={() => navigate(`/leads/${lead.id}`)} style={{ cursor: 'pointer', borderBottom: '1px solid #f3f4f6' }}>
                  <td style={styles.td}>{lead.full_name}</td>
                  <td style={styles.td}>{lead.email}</td>
                  <td style={styles.td}>{lead.company || '-'}</td>
                  <td style={styles.td}>{lead.source}</td>
                  <td style={styles.td}>
                    <span style={{ ...styles.badge, backgroundColor: STATUS_BADGES[lead.status]?.bg, color: STATUS_BADGES[lead.status]?.color }}>
                      {lead.status}
                    </span>
                  </td>
                  <td style={styles.td}>{lead.capture_date?.split('T')[0] || '-'}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {totalPages > 1 && (
        <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', gap: '16px', marginTop: '20px' }}>
          <button onClick={() => setPage(p => Math.max(1, p - 1))} disabled={page === 1} style={styles.btnSecondary}>
            <ChevronLeft size={16} />
          </button>
          <span style={{ fontSize: '14px', color: '#6b7280' }}>Page {page} of {totalPages}</span>
          <button onClick={() => setPage(p => Math.min(totalPages, p + 1))} disabled={page === totalPages} style={styles.btnSecondary}>
            <ChevronRight size={16} />
          </button>
        </div>
      )}
    </div>
  )
}

const styles = {
  table: { background: '#fff', borderRadius: '10px', boxShadow: '0 1px 4px rgba(0,0,0,0.06)', overflow: 'hidden' },
  th: { textAlign: 'left', padding: '14px 16px', fontSize: '13px', fontWeight: '600', color: '#6b7280', textTransform: 'uppercase' },
  td: { padding: '14px 16px', fontSize: '14px' },
  badge: { padding: '4px 10px', borderRadius: '12px', fontSize: '12px', fontWeight: '600', textTransform: 'capitalize' },
  btnPrimary: { display: 'flex', alignItems: 'center', gap: '6px', padding: '10px 16px', background: '#4f46e5', color: '#fff', borderRadius: '6px', fontSize: '14px', fontWeight: '500', textDecoration: 'none' },
  btnSecondary: { display: 'flex', alignItems: 'center', gap: '6px', padding: '10px 16px', background: '#fff', color: '#374151', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '14px', fontWeight: '500' },
}

export default Leads
