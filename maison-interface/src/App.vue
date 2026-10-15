<script setup>
import { computed } from 'vue'
import { etat } from './store'
import StatusBar from './components/StatusBar.vue'
import MeteoCards from './components/MeteoCards.vue'
import LampeCard from './components/LampeCard.vue'
import PorteCard from './components/PorteCard.vue'
import MeteoChart from './components/MeteoChart.vue'
import AccesList from './components/AccesList.vue'
import AlarmesList from './components/AlarmesList.vue'
import BadgesManager from './components/BadgesManager.vue'

// Alarme "récente" = moins de 5 minutes
const alarmeRecente = computed(() => {
  const a = etat.value?.alarmes?.[0]
  if (!a) return null
  const age = Date.now() - new Date(a.date.replace(' ', 'T')).getTime()
  return age < 5 * 60 * 1000 ? a : null
})
</script>

<template>
  <StatusBar />

  <div v-if="alarmeRecente" class="bandeau-alarme">
    ⚠ ALARME : 3 refus consécutifs à {{ alarmeRecente.date.slice(11, 19) }}
  </div>

  <main v-if="etat" class="page">
    <MeteoCards />
    <div class="grille-2">
      <LampeCard />
      <PorteCard />
    </div>
    <MeteoChart />
    <div class="grille-2">
      <AccesList />
      <AlarmesList />
    </div>
    <BadgesManager />
  </main>

  <p v-else class="chargement">Chargement…</p>
</template>