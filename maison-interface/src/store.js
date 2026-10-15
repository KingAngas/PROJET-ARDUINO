import { ref } from 'vue'
import { io } from 'socket.io-client'

export const etat = ref(null)          // tout l'état envoyé par le serveur
export const connecte = ref(false)     // connexion temps réel active ?
export const badgesVersion = ref(0)    // change quand les badges sont modifiés

const socket = io()

socket.on('connect', () => { connecte.value = true })
socket.on('disconnect', () => { connecte.value = false })
socket.on('etat', (e) => { etat.value = e })
socket.on('badges', () => { badgesVersion.value++ })