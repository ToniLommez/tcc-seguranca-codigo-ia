import { useState, useEffect } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { leadsAPI } from '../services/api'

function LeadForm() {
  const { id } = useParams()
  const navigate = useNavigate()
  const isEdit = Boolean(id)

  const [form, setForm] = useState({
    full_name: '',
    email: '',
    phone: '',
    company: '',
    position: '',
    source: 'other',
    status: 'new',
  })
  const [error, setError] = useState('')
  const [loading, setLoading] = useState(false)

  useEffect(() => {
    if (isEdit) {
      leadsAPI.get(id).then((res) => {
        const { full_name, email, phone, company, position, source, status } = res.data
        setForm({ full_name, email, phone: phone || '', company: company || '', position: position || '', source, status })
      }).catch(() => navigate('/leads'))
    }
  }, [id])

  const handleChange = (e) => {
    setForm((prev) => ({ ...prev, [e.target.name]: e.target.value }))
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    setError('')
    setLoading(true)
    try {
      if (isEdit) {
        await leadsAPI.update(id, form)
        navigate(`/leads/${id}`)
      } else {
        const res = await leadsAPI.create(form)
        navigate(`/leads/${res.data.id}`)
      }
    } catch (err) {
      setError(err.response?.data?.detail || 'Failed to save lead')
    }
    setLoading(false)
  }

  return (
    <div style={{ maxWidth: '700px' }}>
      <h2 style={{ fontSize: '24px', fontWeight: '700', marginBottom: '24px' }}>
        {isEdit ? 'Edit Lead' : 'New Lead'}
      </h2>

      {error && <div style={styles.error}>{error}</div>}

      <form onSubmit={handleSubmit} style={styles.form}>
        <div style={styles.row}>
          <div style={styles.field}>
            <label style={styles.label}>Full Name *</label>
            <input name="full_name" value={form.full_name} onChange={handleChange} required />
          </div>
          <div style={styles.field}>
            <label style={styles.label}>Email *</label>
            <input name="email" type="email" value={form.email} onChange={handleChange} required />
          </div>
        </div>

        <div style={styles.row}>
          <div style={styles.field}>
            <label style={styles.label}>Phone</label>
            <input name="phone" value={form.phone} onChange={handleChange} />
          </div>
          <div style={styles.field}>
            <label style={styles.label}>Company</label>
            <input name="company" value={form.company} onChange={handleChange} />
          </div>
        </div>

        <div style={styles.row}>
          <div style={styles.field}>
            <label style={styles.label}>Position</label>
            <input name="position" value={form.position} onChange={handleChange} />
          </div>
          <div style={styles.field}>
            <label style={styles.label}>Source</label>
            <select name="source" value={form.source} onChange={handleChange}>
              <option value="referral">Referral</option>
              <option value="website">Website</option>
              <option value="event">Event</option>
              <option value="social_media">Social Media</option>
              <option value="cold_call">Cold Call</option>
              <option value="other">Other</option>
            </select>
          </div>
        </div>

        <div style={styles.row}>
          <div style={styles.field}>
            <label style={styles.label}>Status</label>
            <select name="status" value={form.status} onChange={handleChange}>
              <option value="new">New</option>
              <option value="contacted">Contacted</option>
              <option value="qualified">Qualified</option>
              <option value="lost">Lost</option>
            </select>
          </div>
          <div style={styles.field}></div>
        </div>

        <div style={{ display: 'flex', gap: '12px', marginTop: '12px' }}>
          <button type="submit" disabled={loading} style={styles.submitBtn}>
            {loading ? 'Saving...' : isEdit ? 'Update Lead' : 'Create Lead'}
          </button>
          <button type="button" onClick={() => navigate(-1)} style={styles.cancelBtn}>Cancel</button>
        </div>
      </form>
    </div>
  )
}

const styles = {
  form: { background: '#fff', padding: '32px', borderRadius: '10px', boxShadow: '0 1px 4px rgba(0,0,0,0.06)' },
  row: { display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '20px' },
  field: {},
  label: { display: 'block', marginBottom: '6px', fontSize: '14px', fontWeight: '500', color: '#374151' },
  submitBtn: { padding: '12px 24px', background: '#4f46e5', color: '#fff', borderRadius: '6px', fontSize: '15px', fontWeight: '600' },
  cancelBtn: { padding: '12px 24px', background: '#fff', color: '#6b7280', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '15px' },
  error: { background: '#fef2f2', color: '#dc2626', padding: '12px', borderRadius: '6px', marginBottom: '20px', fontSize: '14px' },
}

export default LeadForm
