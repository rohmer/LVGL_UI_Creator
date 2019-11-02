pipeline {
  agent {
    node {
      label 'PILinux'
    }

  }
  stages {
    stage('Checkout') {
      steps {
        git(url: 'https://github.com/rohmer/LVGL_UI_Creator', branch: 'Dev', poll: true)
      }
    }
    stage('Build') {
      steps {
        script {
          cmakeBuild
          [
            generator: 'Ninja',
            buildDir: 'build',
            sourceDir: '${WORKSPACE}',
            installation: 'InSearchPath',
            steps: [
              [args: 'all', envVars: 'DESTDIR=${WORKSPACE}/built']
            ]
          ]
        }

      }
    }
  }
}