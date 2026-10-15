async function appel(method, url, body) {
  const options = { method }
  if (body !== undefined) {
    options.headers = { 'Content-Type': 'application/json' }
    options.body = JSON.stringify(body)
  }
  const r = await fetch(url, options)
  const data = await r.json().catch(() => ({}))
  if (!r.ok) throw new Error(data.erreur || 'Erreur serveur')
  return data
}

export const get = (url) => appel('GET', url)
export const post = (url, body) => appel('POST', url, body ?? {})
export const patch = (url, body) => appel('PATCH', url, body)
export const del = (url) => appel('DELETE', url)