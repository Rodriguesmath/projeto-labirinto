import { InfluxDB } from 'https://unpkg.com/@influxdata/influxdb-client-browser/dist/index.browser.mjs';
import { ENV } from './env.js';

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);
const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const baseGeometry = new THREE.BoxGeometry(6, 0.4, 6);
const baseMaterial = new THREE.MeshPhongMaterial({ color: 0x2a2a35, flatShading: true });
const mesa = new THREE.Mesh(baseGeometry, baseMaterial);

const edges = new THREE.EdgesGeometry(baseGeometry);
const line = new THREE.LineSegments(edges, new THREE.LineBasicMaterial({ color: 0x00ffcc }));
mesa.add(line);
scene.add(mesa);

const light = new THREE.DirectionalLight(0xffffff, 1);
light.position.set(5, 10, 7);
scene.add(light);
scene.add(new THREE.AmbientLight(0xffffff, 0.5));

camera.position.set(0, 6, 8);
camera.lookAt(0, 0, 0);

const url = ENV.URL;
const token = ENV.TOKEN; 
const org = ENV.ORG;

const queryApi = new InfluxDB({url, token}).getQueryApi(org);

const fluxQuery = `
  from(bucket: "${ENV.BUCKET}")
    |> range(start: -2s)
    |> filter(fn: (r) => r["_measurement"] == "orientacao_mesa")
    |> filter(fn: (r) => r["_field"] == "pitch_x_deg" or r["_field"] == "roll_y_deg")
    |> last()
`;

let targetPitch = 0;
let targetRoll = 0;

function fetchInflux() {
    queryApi.queryRows(fluxQuery, {
        next(row, tableMeta) {
            const obj = tableMeta.toObject(row);
            //converte os graus recebidos para radianos (unidade usada pelo Three.js)
            if (obj._field === 'pitch_x_deg') targetPitch = obj._value * (Math.PI / 180);
            if (obj._field === 'roll_y_deg') targetRoll = obj._value * (Math.PI / 180);
        },
        error(error) { 
            console.error('Erro na consulta do InfluxDB:', error); 
        },
        complete() {}
    });
}

//ajustar de acordo com a taxa de atualização do grafana (em ms)
setInterval(fetchInflux, 1000);

function animate() {
    requestAnimationFrame(animate);
    
    mesa.rotation.x += (targetPitch - mesa.rotation.x) * 0.1;
    mesa.rotation.z += (-targetRoll - mesa.rotation.z) * 0.1;
    
    renderer.render(scene, camera);
}

animate();

// resize se o painel for redimensionado
window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
});

// sem esse recálculo o modelo 3d não renderiza corretament
window.addEventListener('load', () => {
    setTimeout(() => {
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth, window.innerHeight);
    }, 200);
});