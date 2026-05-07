import axios from 'axios'

const API_BASE = 'http://localhost:8000/api'

const api = axios.create({
  baseURL: API_BASE,
})

api.interceptors.request.use((config) => {
  const token = localStorage.getItem('token')
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      localStorage.removeItem('token')
      localStorage.removeItem('user')
      window.location.href = '/login'
    }
    return Promise.reject(error)
  }
)

export const authAPI = {
  login: (data) => api.post('/auth/login', data),
  register: (data) => api.post('/auth/register', data),
  me: () => api.get('/auth/me'),
}

export const leadsAPI = {
  list: (params) => api.get('/leads', { params }),
  get: (id) => api.get(`/leads/${id}`),
  create: (data) => api.post('/leads', data),
  update: (id, data) => api.put(`/leads/${id}`, data),
  delete: (id) => api.delete(`/leads/${id}`),
  stats: () => api.get('/leads/stats'),
  addInteraction: (id, data) => api.post(`/leads/${id}/interactions`, data),
  importCSV: (file) => {
    const formData = new FormData()
    formData.append('file', file)
    return api.post('/leads/import/csv', formData, {
      headers: { 'Content-Type': 'multipart/form-data' },
    })
  },
  exportCSV: () => api.get('/leads/export/csv', { responseType: 'blob' }),
  exportExcel: () => api.get('/leads/export/excel', { responseType: 'blob' }),
}

export default api
