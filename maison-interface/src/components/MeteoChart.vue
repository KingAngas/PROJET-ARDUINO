<script setup>
import { ref, onMounted, onBeforeUnmount, watch } from 'vue'
import Chart from 'chart.js/auto'
import { get } from '../api'
import { etat } from '../store'

const canvas = ref(null)
const heures = ref(6)
let chart = null

async function charger() {
  if (!chart) return
  try {
    const data = await get(`/api/meteo/historique?heures=${heures.value}`)
    chart.data.labels = data.map((d) => d.date.slice(11, 16))
    chart.data.datasets[0].data = data.map((d) => d.temperature)
    chart.data.datasets[1].data = data.map((d) => d.humidite)
    chart.update()
  } catch (e) {
    console.error(e)
  }
}

onMounted(() => {
  chart = new Chart(canvas.value, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { label: 'Température (°C)', data: [], borderColor: '#e4572e', backgroundColor: '#e4572e', yAxisID: 'yT', tension: 0.3, pointRadius: 2 },
        { label: 'Humidité (%)', data: [], borderColor: '#2e86e4', backgroundColor: '#2e86e4', yAxisID: 'yH', tension: 0.3, pointRadius: 2 }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: 'index', intersect: false },
      scales: {
        yT: { position: 'left', title: { display: true, text: '°C' } },
        yH: { position: 'right', title: { display: true, text: '%' }, grid: { drawOnChartArea: false } }
      }
    }
  })
  charger()
})

// Recharge le graphique à chaque nouvelle mesure
watch(() => etat.value?.meteo?.date, charger)
watch(heures, charger)

onBeforeUnmount(() => chart?.destroy())
</script>

<template>
  <section class="carte">
    <div class="entete">
      <h2>Température et humidité</h2>
      <select v-model.number="heures">
        <option :value="1">1 h</option>
        <option :value="6">6 h</option>
        <option :value="24">24 h</option>
        <option :value="72">3 jours</option>
      </select>
    </div>
    <div class="zone-graphique"><canvas ref="canvas"></canvas></div>
  </section>
</template>