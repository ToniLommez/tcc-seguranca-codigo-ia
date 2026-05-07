import { useState, useEffect } from 'react'
import { useParams, useNavigate, Link } from 'react-router-dom'
import { ArrowLeft, Edit, Trash2, MessageSquare } from 'lucide-react'
import { leadsAPI } from '../services/api'

function LeadDetail() {
  const { id } = useParams()
  const navigate = useNavigate()
  const [lead, setLead] = useState(null)
  const [loading, setLoading] = useState(true)
  const [note, setNote] = useState('')
  const [interactionType, setInteractionType] = useState('note')
  const [submitting, setSubmitting] = useState(false)

  useEffect(() => {
    leadsAPI.get(id).then((res) => {
      setLead(res.data)
      setLoading(false)
    }).catch(() => {
      setLoading(false)
      navigate('/leads')
    })
  }, [id])

  const handleDelete = async () => {
    if (!confirm('Are you sure you want to delete this lead?')) return
    await leadsAPI.delete(id)
    navigate('/leads')
  }

  const handleAddInteraction = async (e) => {
    e.preventDefault()
    if (!note.trim()) return
    setSubmitting(true)
    try {
      const res = await leadsAPI.addInteraction(id, { note, interaction_type: interactionType })
      setLead((prev) => ({ ...prev, interactions: [...(prev.interactions || []), res.data] }))
      setNote('')
    } catch (err) {
      alert('Failed to add interaction')
    }
    setSubmitting(false)
  }

  if (loading) return <p>Loading...</p>
  if (!lead) return <p>Lead not found.</p>

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px' }}>
        <button onClick={() => navigate('/leads')} style={styles.backBtn}><ArrowLeft size={16} /> Back to Leads</button>
        <div style={{ display: 'flex', gap: '10px' }}>
          <Link to={`/leads/${id}/edit`} style={styles.editBtn}><Edit size={16} /> Edit</Link>
          <button onClick={handleDelete} style={styles.deleteBtn}><Trash2 size={16} /> Delete</button>
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '24px' }}>
        <div style={styles.card}>
          <h3 style={styles.cardTitle}>Lead Information</h3>
          <div style={styles.fieldGrid}>
            <Field label="Full Name" value={lead.full_name} />
            <Field label="Email" value={lead.email} />
            <Field label="Phone" value={lead.phone || '-'} />
            <Field label="Company" value={lead.company || '-'} />
            <Field label="Position" value={lead.position || '-'} />
            <Field label="Source" value={lead.source} />
            <Field label="Status" value={lead.status} />
            <Field label="Capture Date" value={lead.capture_date?.split('T')[0] || '-'} />
          </div>
        </div>

        <div style={styles.card}>
          <h3 style={styles.cardTitle}>
            <MessageSquare size={18} /> Interaction History
          </h3>

          <form onSubmit={handleAddInteraction} style={{ marginBottom: '20px' }}>
            <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
              <select value={interactionType} onChange={(e) => setInteractionType(e.target.value)} style={{ width: '140px' }}>
                <option value="note">Note</option>
                <option value="call">Call</option>
                <option value="email">Email</option>
                <option value="meeting">Meeting</option>
              </select>
              <input placeholder="Add a note..." value={note} onChange={(e) => setNote(e.target.value)} style={{ flex: 1 }} />
              <button type="submit" disabled={submitting} style={styles.addBtn}>Add</button>
            </div>
          </form>

          <div style={{ maxHeight: '400px', overflow: 'auto' }}>
            {(!lead.interactions || lead.interactions.length === 0) ? (
              <p style={{ color: '#9ca3af', fontSize: '14px' }}>No interactions yet.</p>
            ) : (
              [...lead.interactions].reverse().map((interaction, idx) => (
                <div key={idx} style={styles.interaction}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <span style={styles.interactionType}>{interaction.interaction_type}</span>
                    <span style={{ fontSize: '12px', color: '#9ca3af' }}>{interaction.created_at?.split('T')[0]}</span>
                  </div>
                  <p style={{ fontSize: '14px', color: '#374151' }}>{interaction.note}</p>
                  <p style={{ fontSize: '12px', color: '#9ca3af', marginTop: '4px' }}>by {interaction.created_by}</p>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  )
}

function Field({ label, value }) {
  return (
    <div>
      <p style={{ fontSize: '12px', color: '#6b7280', marginBottom: '2px' }}>{label}</p>
      <p style={{ fontSize: '14px', fontWeight: '500', textTransform: 'capitalize' }}>{value}</p>
    </div>
  )
}

const styles = {
  card: { background: '#fff', padding: '24px', borderRadius: '10px', boxShadow: '0 1px 4px rgba(0,0,0,0.06)' },
  cardTitle: { fontSize: '16px', fontWeight: '600', marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '8px' },
  fieldGrid: { display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' },
  backBtn: { display: 'flex', alignItems: 'center', gap: '6px', padding: '8px 14px', background: 'none', color: '#6b7280', fontSize: '14px' },
  editBtn: { display: 'flex', alignItems: 'center', gap: '6px', padding: '8px 14px', background: '#4f46e5', color: '#fff', borderRadius: '6px', fontSize: '14px', textDecoration: 'none' },
  deleteBtn: { display: 'flex', alignItems: 'center', gap: '6px', padding: '8px 14px', background: '#ef4444', color: '#fff', borderRadius: '6px', fontSize: '14px' },
  addBtn: { padding: '8px 16px', background: '#4f46e5', color: '#fff', borderRadius: '6px', fontSize: '14px' },
  interaction: { padding: '12px', borderLeft: '3px solid #e5e7eb', marginBottom: '12px', background: '#f9fafb', borderRadius: '0 6px 6px 0' },
  interactionType: { fontSize: '12px', fontWeight: '600', textTransform: 'uppercase', color: '#4f46e5' },
}

export default LeadDetail
